// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/service_manager_connection_impl.h"

#include <queue>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/message_loop/message_loop_current.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ios/web/public/web_thread.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/embedded_service_runner.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/mojom/constants.mojom.h"
#include "services/service_manager/public/mojom/service_factory.mojom.h"

namespace web {
namespace {

base::LazyInstance<std::unique_ptr<ServiceManagerConnection>>::Leaky
    g_connection_for_process = LAZY_INSTANCE_INITIALIZER;

}  // namespace

// A ref-counted object which owns the IO thread state of a
// ServiceManagerConnectionImpl. This includes Service and ServiceFactory
// bindings.
class ServiceManagerConnectionImpl::IOThreadContext
    : public base::RefCountedThreadSafe<IOThreadContext>,
      public service_manager::Service,
      public service_manager::mojom::ServiceFactory {
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
        FROM_HERE, base::Bind(&IOThreadContext::StartOnIOThread, this));
  }

  // Safe to call from whichever thread called Start() (or may have called
  // Start()). Must be called before IO thread shutdown.
  void ShutDown() {
    if (!started_)
      return;

    bool posted = io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&IOThreadContext::ShutDownOnIOThread, this));
    DCHECK(posted);
  }

  void AddEmbeddedService(const std::string& name,
                          const service_manager::EmbeddedServiceInfo& info) {
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&ServiceManagerConnectionImpl::IOThreadContext::
                                  AddEmbeddedServiceRequestHandlerOnIoThread,
                              this, name, info));
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

  void StartOnIOThread() {
    // Should bind |io_thread_checker_| to the context's thread.
    DCHECK(io_thread_checker_.CalledOnValidThread());
    DCHECK(!service_context_);
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

    request_handlers_.clear();
    embedded_services_.clear();
  }

  void AddEmbeddedServiceRequestHandlerOnIoThread(
      const std::string& name,
      const service_manager::EmbeddedServiceInfo& info) {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    auto service =
        std::make_unique<service_manager::EmbeddedServiceRunner>(name, info);
    AddServiceRequestHandlerOnIoThread(
        name,
        base::Bind(&service_manager::EmbeddedServiceRunner::BindServiceRequest,
                   base::Unretained(service.get())));
    auto insertion_result =
        embedded_services_.insert(std::make_pair(name, std::move(service)));
    DCHECK(insertion_result.second);
  }

  void AddServiceRequestHandlerOnIoThread(
      const std::string& name,
      const ServiceRequestHandler& handler) {
    DCHECK(io_thread_checker_.CalledOnValidThread());
    auto result = request_handlers_.insert(std::make_pair(name, handler));
    DCHECK(result.second);
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
    }
  }

  bool OnServiceManagerConnectionLost() override {
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
    it->second.Run(std::move(request));
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

  std::map<std::string, std::unique_ptr<service_manager::EmbeddedServiceRunner>>
      embedded_services_;
  std::map<std::string, ServiceRequestHandler> request_handlers_;

  base::WeakPtrFactory<IOThreadContext> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IOThreadContext);
};

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnection, public:

// static
void ServiceManagerConnection::Set(
    std::unique_ptr<ServiceManagerConnection> connection) {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(!g_connection_for_process.Get());
  g_connection_for_process.Get() = std::move(connection);
}

// static
ServiceManagerConnection* ServiceManagerConnection::Get() {
  // WebThreads are not initialized in many unit tests. These tests also by
  // definition are not setting the global ServiceManagerConnection (since
  // otherwise the DCHECK in the above method would fire).
  DCHECK(!web::WebThread::IsThreadInitialized(web::WebThread::UI) ||
         web::WebThread::CurrentlyOn(web::WebThread::UI));
  return g_connection_for_process.Get().get();
}

// static
void ServiceManagerConnection::Destroy() {
  DCHECK_CURRENTLY_ON(WebThread::UI);

  // This joins the service manager controller thread.
  g_connection_for_process.Get().reset();
}

// static
std::unique_ptr<ServiceManagerConnection> ServiceManagerConnection::Create(
    service_manager::mojom::ServiceRequest request,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner) {
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
  context_ = new IOThreadContext(std::move(request), io_task_runner,
                                 std::move(io_thread_connector),
                                 std::move(connector_request));
}

ServiceManagerConnectionImpl::~ServiceManagerConnectionImpl() {
  context_->ShutDown();
}

////////////////////////////////////////////////////////////////////////////////
// ServiceManagerConnectionImpl, ServiceManagerConnection implementation:

void ServiceManagerConnectionImpl::Start() {
  context_->Start(base::Bind(&ServiceManagerConnectionImpl::OnConnectionLost,
                             weak_factory_.GetWeakPtr()));
}

service_manager::Connector* ServiceManagerConnectionImpl::GetConnector() {
  return connector_.get();
}

void ServiceManagerConnectionImpl::AddEmbeddedService(
    const std::string& name,
    const service_manager::EmbeddedServiceInfo& info) {
  context_->AddEmbeddedService(name, info);
}

void ServiceManagerConnectionImpl::OnConnectionLost() {}

void ServiceManagerConnectionImpl::GetInterface(
    service_manager::mojom::InterfaceProvider* provider,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle request_handle) {
  provider->GetInterface(interface_name, std::move(request_handle));
}

}  // namespace web
