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
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/thread_annotations.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "content/common/child.mojom.h"
#include "content/public/common/connection_filter.h"
#include "content/public/common/service_names.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/embedded_service_runner.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"
#include "services/service_manager/runner/common/client_util.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "jni/ServiceManagerConnectionImpl_jni.h"
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
      public service_manager::Service,
      public service_manager::mojom::ServiceFactory,
      public mojom::Child {
 public:
  IOThreadContext(
      service_manager::mojom::ServiceRequest service_request,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner,
      std::unique_ptr<service_manager::Connector> io_thread_connector,
      service_manager::mojom::ConnectorRequest connector_request)
      : pending_service_request_(std::move(service_request)),
        io_task_runner_(io_task_runner),
        io_thread_connector_(std::move(io_thread_connector)),
        pending_connector_request_(std::move(connector_request)),
        child_binding_(this),
        weak_factory_(this) {
    // This will be reattached by any of the IO thread functions on first call.
    io_thread_checker_.DetachFromThread();
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

  void AddEmbeddedService(const std::string& name,
                          const service_manager::EmbeddedServiceInfo& info) {
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ServiceManagerConnectionImpl::IOThreadContext::
                           AddEmbeddedServiceRequestHandlerOnIoThread,
                       this, name, info));
  }

  void AddServiceRequestHandler(const std::string& name,
                                const ServiceRequestHandler& handler) {
    AddServiceRequestHandlerWithPID(
        name, base::BindRepeating(&WrapServiceRequestHandlerNoPID, handler));
  }

  void AddServiceRequestHandlerWithPID(
      const std::string& name,
      const ServiceRequestHandlerWithPID& handler) {
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

  static void WrapServiceRequestHandlerNoPID(
      const ServiceRequestHandler& handler,
      service_manager::mojom::ServiceRequest request,
      service_manager::mojom::PIDReceiverPtr pid_receiver) {
    handler.Run(std::move(request));
  }

  void StartOnIOThread() {
    // Should bind |io_thread_checker_| to the context's thread.
    DCHECK(io_thread_checker_.CalledOnValidThread());
    service_context_.reset(new service_manager::ServiceContext(
        std::make_unique<service_manager::ForwardingService>(this),
        std::move(pending_service_request_), std::move(io_thread_connector_),
        std::move(pending_connector_request_)));

    // MessageLoopObserver owns itself.
    message_loop_observer_ =
        new MessageLoopObserver(weak_factory_.GetWeakPtr());
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

    factory_bindings_.CloseAllBindings();
    service_context_.reset();

    ClearConnectionFiltersOnIOThread();

    request_handlers_.clear();
    embedded_services_.clear();
    child_binding_.Close();
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

  void AddEmbeddedServiceRequestHandlerOnIoThread(
      const std::string& name,
      const service_manager::EmbeddedServiceInfo& info) {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    std::unique_ptr<service_manager::EmbeddedServiceRunner> service(
        new service_manager::EmbeddedServiceRunner(name, info));
    AddServiceRequestHandlerOnIoThread(
        name,
        base::BindRepeating(
            &WrapServiceRequestHandlerNoPID,
            base::BindRepeating(
                &service_manager::EmbeddedServiceRunner::BindServiceRequest,
                base::Unretained(service.get()))));
    auto result =
        embedded_services_.insert(std::make_pair(name, std::move(service)));
    DCHECK(result.second);
  }

  void AddServiceRequestHandlerOnIoThread(
      const std::string& name,
      const ServiceRequestHandlerWithPID& handler) {
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
    if (source_info.identity.name() == service_manager::mojom::kServiceName &&
        interface_name == service_manager::mojom::ServiceFactory::Name_) {
      factory_bindings_.AddBinding(
          this, service_manager::mojom::ServiceFactoryRequest(
                    std::move(interface_pipe)));
    } else if (source_info.identity.name() == mojom::kBrowserServiceName &&
               interface_name == mojom::Child::Name_) {
      DCHECK(!child_binding_.is_bound());
      child_binding_.Bind(mojom::ChildRequest(std::move(interface_pipe)));
    } else {
      base::AutoLock lock(lock_);
      for (auto& entry : connection_filters_) {
        entry.second->OnBindInterface(source_info, interface_name,
                                      &interface_pipe,
                                      service_context_->connector());
        // A filter may have bound the interface, claiming the pipe.
        if (!interface_pipe.is_valid())
          return;
      }
    }
  }

  bool OnServiceManagerConnectionLost() override {
    ClearConnectionFiltersOnIOThread();
    callback_task_runner_->PostTask(FROM_HERE, stop_callback_);
    return true;
  }

  /////////////////////////////////////////////////////////////////////////////
  // service_manager::mojom::ServiceFactory:

  void CreateService(
      service_manager::mojom::ServiceRequest request,
      const std::string& name,
      service_manager::mojom::PIDReceiverPtr pid_receiver) override {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    auto it = request_handlers_.find(name);
    if (it == request_handlers_.end()) {
      LOG(ERROR) << "Can't create service " << name << ". No handler found.";
      return;
    }
    it->second.Run(std::move(request), std::move(pid_receiver));
  }

  base::ThreadChecker io_thread_checker_;
  bool started_ = false;

  // Temporary state established on construction and consumed on the IO thread
  // once the connection is started.
  service_manager::mojom::ServiceRequest pending_service_request_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<service_manager::Connector> io_thread_connector_;
  service_manager::mojom::ConnectorRequest pending_connector_request_;

  // TaskRunner on which to run our owner's callbacks, i.e. the ones passed to
  // Start().
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  // Callback to run if the service is stopped by the service manager.
  base::Closure stop_callback_;

  std::unique_ptr<service_manager::ServiceContext> service_context_;
  mojo::BindingSet<service_manager::mojom::ServiceFactory> factory_bindings_;

  // Not owned.
  MessageLoopObserver* message_loop_observer_ = nullptr;

  // Guards |connection_filters_| and |next_filter_id_|.
  base::Lock lock_;
  std::map<int, std::unique_ptr<ConnectionFilter>> connection_filters_
      GUARDED_BY(lock_);
  int next_filter_id_ GUARDED_BY(lock_) = kInvalidConnectionFilterId;

  std::map<std::string, std::unique_ptr<service_manager::EmbeddedServiceRunner>>
      embedded_services_;
  std::map<std::string, ServiceRequestHandlerWithPID> request_handlers_;

  mojo::Binding<mojom::Child> child_binding_;

  base::WeakPtrFactory<IOThreadContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadContext);
};

#if defined(OS_ANDROID)
// static
jint JNI_ServiceManagerConnectionImpl_GetConnectorMessagePipeHandle(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller) {
  DCHECK(ServiceManagerConnection::GetForProcess());

  service_manager::mojom::ConnectorPtrInfo connector_info;
  ServiceManagerConnection::GetForProcess()
      ->GetConnector()
      ->BindConnectorRequest(mojo::MakeRequest(&connector_info));

  return connector_info.PassHandle().release().value();
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
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : weak_factory_(this) {
  service_manager::mojom::ConnectorRequest connector_request;
  connector_ = service_manager::Connector::Create(&connector_request);

  std::unique_ptr<service_manager::Connector> io_thread_connector =
      connector_->Clone();
  context_ = new IOThreadContext(
      std::move(request), io_task_runner, std::move(io_thread_connector),
      std::move(connector_request));
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

void ServiceManagerConnectionImpl::AddEmbeddedService(
    const std::string& name,
    const service_manager::EmbeddedServiceInfo& info) {
  context_->AddEmbeddedService(name, info);
}

void ServiceManagerConnectionImpl::AddServiceRequestHandler(
    const std::string& name,
    const ServiceRequestHandler& handler) {
  context_->AddServiceRequestHandler(name, handler);
}

void ServiceManagerConnectionImpl::AddServiceRequestHandlerWithPID(
    const std::string& name,
    const ServiceRequestHandlerWithPID& handler) {
  context_->AddServiceRequestHandlerWithPID(name, handler);
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
