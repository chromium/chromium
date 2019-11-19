// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/service_manager/service_manager_connection_impl.h"

#include <map>
#include <queue>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_binding.h"
#include "services/service_manager/public/mojom/constants.mojom.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "content/public/android/content_jni_headers/ServiceManagerConnectionImpl_jni.h"
#include "services/service_manager/public/cpp/connector.h"
#include "services/service_manager/public/mojom/connector.mojom.h"
#endif

namespace content {
namespace {

base::LazyInstance<std::unique_ptr<ServiceManagerConnection>>::Leaky
    g_connection_for_process = LAZY_INSTANCE_INITIALIZER;

ServiceManagerConnection::Factory* service_manager_connection_factory = nullptr;

}  // namespace

// A ref-counted object which owns the IO thread state of a
// ServiceManagerConnectionImpl. This includes Service and ServiceFactory
// bindings.
class ServiceManagerConnectionImpl::IOThreadContext
    : public base::RefCountedThreadSafe<IOThreadContext>,
      public service_manager::Service {
 public:
  IOThreadContext(service_manager::mojom::ServiceRequest service_request,
                  scoped_refptr<base::SequencedTaskRunner> io_task_runner,
                  mojo::PendingReceiver<service_manager::mojom::Connector>
                      connector_receiver)
      : pending_service_request_(std::move(service_request)),
        io_task_runner_(io_task_runner),
        pending_connector_receiver_(std::move(connector_receiver)) {
    // This will be reattached by any of the IO thread functions on first call.
    io_thread_checker_.DetachFromThread();
  }

  void SetDefaultServiceRequestHandler(
      const ServiceManagerConnection::DefaultServiceRequestHandler& handler) {
    DCHECK(!started_);
    default_request_handler_ = handler;
  }

  // Safe to call from any thread.
  void Start(const base::Closure& stop_callback) {
    DCHECK(!started_);

    started_ = true;
    callback_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    stop_callback_ = stop_callback;
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadContext::StartOnIOThread, this));
  }

  void Stop() {
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadContext::StopOnIOThread, this));
  }

  // Safe to call from whichever thread called Start() (or may have called
  // Start()). Must be called before IO thread shutdown.
  void ShutDown() {
    if (!started_)
      return;

    bool posted = io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&IOThreadContext::ShutDownOnIOThread, this));
    DCHECK(posted);
  }

  // Safe to call any time before a message is received from a process.
  // i.e. can be called when starting the process but not afterwards.
  int AddConnectionFilter(std::unique_ptr<ConnectionFilter> filter) {
    base::AutoLock lock(lock_);

    int id = ++next_filter_id_;

    // We should never hit this in practice, but let's crash just in case.
    CHECK_NE(id, kInvalidConnectionFilterId);

    connection_filters_[id] = std::move(filter);
    return id;
  }

  void RemoveConnectionFilter(int filter_id) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&IOThreadContext::RemoveConnectionFilterOnIOThread, this,
                       filter_id));
  }

  void AddServiceRequestHandler(const std::string& name,
                                const ServiceRequestHandler& handler) {
    AddServiceRequestHandlerWithCallback(
        name,
        base::BindRepeating(&WrapServiceRequestHandlerNoCallback, handler));
  }

  void AddServiceRequestHandlerWithCallback(
      const std::string& name,
      const ServiceRequestHandlerWithCallback& handler) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceManagerConnectionImpl::IOThreadContext::
                           AddServiceRequestHandlerOnIoThread,
                       this, name, handler));
  }

 private:
  friend class base::RefCountedThreadSafe<IOThreadContext>;

  class MessageLoopObserver
      : public base::MessageLoopCurrent::DestructionObserver {
   public:
    explicit MessageLoopObserver(base::WeakPtr<IOThreadContext> context)
        : context_(context) {
      base::MessageLoopCurrent::Get()->AddDestructionObserver(this);
    }

    ~MessageLoopObserver() override {
      base::MessageLoopCurrent::Get()->RemoveDestructionObserver(this);
    }

    void ShutDown() {
      if (!is_active_)
        return;

      // The call into |context_| below may reenter ShutDown(), hence we set
      // |is_active_| to false here.
      is_active_ = false;
      if (context_)
        context_->ShutDownOnIOThread();

      delete this;
    }

   private:
    void WillDestroyCurrentMessageLoop() override {
      DCHECK(is_active_);
      ShutDown();
    }

    bool is_active_ = true;
    base::WeakPtr<IOThreadContext> context_;

    DISALLOW_COPY_AND_ASSIGN(MessageLoopObserver);
  };

  ~IOThreadContext() override {}

  static void WrapServiceRequestHandlerNoCallback(
      const ServiceRequestHandler& handler,
      service_manager::mojom::ServiceRequest request,
      CreatePackagedServiceInstanceCallback callback) {
    handler.Run(std::move(request));
    std::move(callback).Run(base::GetCurrentProcId());
  }

  void StartOnIOThread() {
    // Should bind |io_thread_checker_| to the context's thread.
    DCHECK(io_thread_checker_.CalledOnValidThread());
    service_binding_ = std::make_unique<service_manager::ServiceBinding>(
        this, std::move(pending_service_request_));
    service_binding_->GetConnector()->BindConnectorReceiver(
        std::move(pending_connector_receiver_));

    // MessageLoopObserver owns itself.
    message_loop_observer_ =
        new MessageLoopObserver(weak_factory_.GetWeakPtr());
  }

  void StopOnIOThread() {
    ClearConnectionFiltersOnIOThread();
    request_handlers_.clear();
  }

  void ShutDownOnIOThread() {
    DCHECK(io_thread_checker_.CalledOnValidThread());

    weak_factory_.InvalidateWeakPtrs();

    // Note that this method may be invoked by MessageLoopObserver observing
    // MessageLoop destruction. In that case, this call to ShutDown is
    // effectively a no-op. In any case it's safe.
    if (message_loop_observer_) {
      message_loop_observer_->ShutDown();
      message_loop_observer_ = nullptr;
    }

    // Resetting the ServiceContext below may otherwise release the last
    // reference to this IOThreadContext. We keep it alive until the stack
    // unwinds.
    scoped_refptr<IOThreadContext> keepalive(this);

    service_binding_.reset();

    StopOnIOThread();
  }

  void ClearConnectionFiltersOnIOThread() {
    base::AutoLock lock(lock_);
    connection_filters_.clear();
  }

  void RemoveConnectionFilterOnIOThread(int filter_id) {
    base::AutoLock lock(lock_);
    auto it = connection_filters_.find(filter_id);
    // During shutdown the connection filters might have been cleared already
    // by ClearConnectionFiltersOnIOThread() above, so this id might not exist.
    if (it != connection_filters_.end())
      connection_filters_.erase(it);
  }

  void AddServiceRequestHandlerOnIoThread(
      const std::string& name,
      const ServiceRequestHandlerWithCallback& handler) {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    auto result = request_handlers_.insert(std::make_pair(name, handler));
    DCHECK(result.second) << "ServiceRequestHandler for " << name
                          << " already exists.";
  }

  /////////////////////////////////////////////////////////////////////////////
  // service_manager::Service implementation

  void OnBindInterface(const service_manager::BindSourceInfo& source_info,
                       const std::string& interface_name,
                       mojo::ScopedMessagePipeHandle interface_pipe) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    base::AutoLock lock(lock_);
    for (auto& entry : connection_filters_) {
      entry.second->OnBindInterface(source_info, interface_name,
                                    &interface_pipe,
                                    service_binding_->GetConnector());
      // A filter may have bound the interface, claiming the pipe.
      if (!interface_pipe.is_valid())
        return;
    }
  }

  void CreatePackagedServiceInstance(
      const std::string& service_name,
      mojo::PendingReceiver<service_manager::mojom::Service> receiver,
      CreatePackagedServiceInstanceCallback callback) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    service_manager::mojom::ServiceRequest request(std::move(receiver));
    auto it = request_handlers_.find(service_name);
    if (it == request_handlers_.end()) {
      if (default_request_handler_) {
        callback_task_runner_->PostTask(
            FROM_HERE, base::BindOnce(default_request_handler_, service_name,
                                      std::move(request)));
      } else {
        LOG(ERROR) << "Can't create service " << service_name
                   << ". No handler found.";
      }
      std::move(callback).Run(base::nullopt);
    } else {
      it->second.Run(std::move(request), std::move(callback));
    }
  }

  void OnDisconnected() override {
    ClearConnectionFiltersOnIOThread();
    callback_task_runner_->PostTask(FROM_HERE, stop_callback_);
  }

  base::ThreadChecker io_thread_checker_;
  bool started_ = false;

  ServiceManagerConnection::DefaultServiceRequestHandler
      default_request_handler_;

  // Temporary state established on construction and consumed on the IO thread
  // once the connection is started.
  service_manager::mojom::ServiceRequest pending_service_request_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  mojo::PendingReceiver<service_manager::mojom::Connector>
      pending_connector_receiver_;

  // TaskRunner on which to run our owner's callbacks, i.e. the ones passed to
  // Start().
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  // Callback to run if the service is stopped by the service manager.
  base::Closure stop_callback_;

  std::unique_ptr<service_manager::ServiceBinding> service_binding_;

  // Not owned.
  MessageLoopObserver* message_loop_observer_ = nullptr;

  // Guards |connection_filters_| and |next_filter_id_|.
  base::Lock lock_;
  std::map<int, std::unique_ptr<ConnectionFilter>> connection_filters_
      GUARDED_BY(lock_);
  int next_filter_id_ GUARDED_BY(lock_) = kInvalidConnectionFilterId;

  std::map<std::string, ServiceRequestHandlerWithCallback> request_handlers_;

  base::WeakPtrFactory<IOThreadContext> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(IOThreadContext);
};

#if defined(OS_ANDROID)
// static
jint JNI_ServiceManagerConnectionImpl_GetConnectorMessagePipeHandle(
    JNIEnv* env) {
  DCHECK(ServiceManagerConnection::GetForProcess());

  mojo::PendingRemote<service_manager::mojom::Connector> connector_remote;
  ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindConnectorReceiver(
          connector_remote.InitWithNewPipeAndPassReceiver());

  return connector_remote.PassPipe().release().value();
}

#endif

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnection, public:

// static
void ServiceManagerConnection::SetForProcess(
    std::unique_ptr<ServiceManagerConnection> connection) {
  DCHECK(!g_connection_for_process.Get());
  g_connection_for_process.Get() = std::move(connection);
}

// static
ServiceManagerConnection* ServiceManagerConnection::GetForProcess() {
  return g_connection_for_process.Get().get();
}

// static
void ServiceManagerConnection::DestroyForProcess() {
  // This joins the service manager controller thread.
  g_connection_for_process.Get().reset();
}

// static
void ServiceManagerConnection::SetFactoryForTest(Factory* factory) {
  DCHECK(!g_connection_for_process.Get());
  service_manager_connection_factory = factory;
}

// static
std::unique_ptr<ServiceManagerConnection> ServiceManagerConnection::Create(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  if (service_manager_connection_factory)
    return service_manager_connection_factory->Run();
  return std::make_unique<ServiceManagerConnectionImpl>(std::move(request),
                                                        io_task_runner);
}

ServiceManagerConnection::~ServiceManagerConnection() {}

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnectionImpl, public:

ServiceManagerConnectionImpl::ServiceManagerConnectionImpl(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
  mojo::PendingReceiver<service_manager::mojom::Connector> connector_receiver;
  connector_ = service_manager::Connector::Create(&connector_receiver);
  context_ = new IOThreadContext(std::move(request), io_task_runner,
                                 std::move(connector_receiver));
}

ServiceManagerConnectionImpl::~ServiceManagerConnectionImpl() {
  context_->ShutDown();
}

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnectionImpl, ServiceManagerConnection implementation:

void ServiceManagerConnectionImpl::Start() {
  context_->Start(
      base::Bind(&ServiceManagerConnectionImpl::OnConnectionLost,
                 weak_factory_.GetWeakPtr()));
}

void ServiceManagerConnectionImpl::Stop() {
  context_->Stop();
}

service_manager::Connector* ServiceManagerConnectionImpl::GetConnector() {
  return connector_.get();
}

void ServiceManagerConnectionImpl::SetConnectionLostClosure(
    const base::Closure& closure) {
  connection_lost_handler_ = closure;
}

int ServiceManagerConnectionImpl::AddConnectionFilter(
    std::unique_ptr<ConnectionFilter> filter) {
  return context_->AddConnectionFilter(std::move(filter));
}

void ServiceManagerConnectionImpl::RemoveConnectionFilter(int filter_id) {
  context_->RemoveConnectionFilter(filter_id);
}

void ServiceManagerConnectionImpl::AddServiceRequestHandler(
    const std::string& name,
    const ServiceRequestHandler& handler) {
  context_->AddServiceRequestHandler(name, handler);
}

void ServiceManagerConnectionImpl::AddServiceRequestHandlerWithCallback(
    const std::string& name,
    const ServiceRequestHandlerWithCallback& handler) {
  context_->AddServiceRequestHandlerWithCallback(name, handler);
}

void ServiceManagerConnectionImpl::SetDefaultServiceRequestHandler(
    const DefaultServiceRequestHandler& handler) {
  context_->SetDefaultServiceRequestHandler(handler);
}

void ServiceManagerConnectionImpl::OnConnectionLost() {
  if (!connection_lost_handler_.is_null())
    connection_lost_handler_.Run();
}

void ServiceManagerConnectionImpl::GetInterface(
    service_manager::mojom::InterfaceProvider* provider,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle request_handle) {
  provider->GetInterface(interface_name, std::move(request_handle));
}

}  // namespace content
