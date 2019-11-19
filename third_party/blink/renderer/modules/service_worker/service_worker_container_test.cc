// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/service_worker/service_worker_container.h"

#include <memory>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_provider.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/bindings/core/v8/dictionary.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/modules/service_worker/navigator_service_worker.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {
namespace {

// Promise-related test support.

struct StubScriptFunction {
 public:
  StubScriptFunction() : call_count_(0) {}

  // The returned ScriptFunction can outlive the StubScriptFunction,
  // but it should not be called after the StubScriptFunction dies.
  v8::Local<v8::Function> GetFunction(ScriptState* script_state) {
    return ScriptFunctionImpl::CreateFunction(script_state, *this);
  }

  size_t CallCount() { return call_count_; }
  ScriptValue Arg() { return arg_; }
  void Trace(blink::Visitor* visitor) { visitor->Trace(arg_); }

 private:
  size_t call_count_;
  ScriptValue arg_;

  class ScriptFunctionImpl : public ScriptFunction {
   public:
    static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                  StubScriptFunction& owner) {
      ScriptFunctionImpl* self =
          MakeGarbageCollected<ScriptFunctionImpl>(script_state, owner);
      return self->BindToV8Function();
    }

    ScriptFunctionImpl(ScriptState* script_state, StubScriptFunction& owner)
        : ScriptFunction(script_state), owner_(owner) {}

   private:
    ScriptValue Call(ScriptValue arg) override {
      owner_.arg_ = arg;
      owner_.call_count_++;
      return ScriptValue();
    }

    StubScriptFunction& owner_;
  };
};

class ScriptValueTest {
 public:
  virtual ~ScriptValueTest() = default;
  virtual void operator()(ScriptState*, ScriptValue) const = 0;
};

// Runs microtasks and expects |promise| to be rejected. Calls
// |valueTest| with the value passed to |reject|, if any.
void ExpectRejected(ScriptState* script_state,
                    ScriptPromise& promise,
                    const ScriptValueTest& value_test) {
  StubScriptFunction resolved, rejected;
  promise.Then(resolved.GetFunction(script_state),
               rejected.GetFunction(script_state));
  v8::MicrotasksScope::PerformCheckpoint(promise.GetIsolate());
  EXPECT_EQ(0ul, resolved.CallCount());
  EXPECT_EQ(1ul, rejected.CallCount());
  if (rejected.CallCount())
    value_test(script_state, rejected.Arg());
}

// DOM-related test support.

// Matches a ScriptValue and a DOMException with a specific name and message.
class ExpectDOMException : public ScriptValueTest {
 public:
  ExpectDOMException(const String& expected_name,
                     const String& expected_message)
      : expected_name_(expected_name), expected_message_(expected_message) {}

  ~ExpectDOMException() override = default;

  void operator()(ScriptState* script_state, ScriptValue value) const override {
    DOMException* exception = V8DOMException::ToImplWithTypeCheck(
        script_state->GetIsolate(), value.V8Value());
    EXPECT_TRUE(exception) << "the value should be a DOMException";
    if (!exception)
      return;
    EXPECT_EQ(expected_name_, exception->name());
    EXPECT_EQ(expected_message_, exception->message());
  }

 private:
  String expected_name_;
  String expected_message_;
};

// Matches a ScriptValue and a TypeError with a message.
class ExpectTypeError : public ScriptValueTest {
 public:
  ExpectTypeError(const String& expected_message)
      : expected_message_(expected_message) {}

  ~ExpectTypeError() override = default;

  void operator()(ScriptState* script_state, ScriptValue value) const override {
    v8::Isolate* isolate = script_state->GetIsolate();
    v8::Local<v8::Context> context = script_state->GetContext();
    v8::Local<v8::Object> error_object =
        value.V8Value()->ToObject(context).ToLocalChecked();
    v8::Local<v8::Value> name =
        error_object->Get(context, V8String(isolate, "name")).ToLocalChecked();
    v8::Local<v8::Value> message =
        error_object->Get(context, V8String(isolate, "message"))
            .ToLocalChecked();

    EXPECT_EQ("TypeError",
              ToCoreString(name->ToString(context).ToLocalChecked()));
    EXPECT_EQ(expected_message_,
              ToCoreString(message->ToString(context).ToLocalChecked()));
  }

 private:
  String expected_message_;
};

// Service Worker-specific tests.

class NotReachedWebServiceWorkerProvider : public WebServiceWorkerProvider {
 public:
  ~NotReachedWebServiceWorkerProvider() override = default;

  void RegisterServiceWorker(
      const WebURL& scope,
      const WebURL& script_url,
      blink::mojom::ScriptType script_type,
      mojom::ServiceWorkerUpdateViaCache update_via_cache,
      const WebFetchClientSettingsObject& fetch_client_settings_object,
      std::unique_ptr<WebServiceWorkerRegistrationCallbacks> callbacks)
      override {
    ADD_FAILURE()
        << "the provider should not be called to register a Service Worker";
  }

  bool ValidateScopeAndScriptURL(const WebURL& scope,
                                 const WebURL& script_url,
                                 WebString* error_message) override {
    return true;
  }
};

class ServiceWorkerContainerTest : public PageTestBase {
 protected:
  void SetUp() override { PageTestBase::SetUp(IntSize()); }

  ~ServiceWorkerContainerTest() override {
    V8GCController::CollectAllGarbageForTesting(GetIsolate());
  }

  v8::Isolate* GetIsolate() { return v8::Isolate::GetCurrent(); }
  ScriptState* GetScriptState() {
    return ToScriptStateForMainWorld(GetDocument().GetFrame());
  }

  void SetPageURL(const String& url) {
    NavigateTo(KURL(NullURL(), url));

    if (url.StartsWith("https://") || url.StartsWith("http://localhost/")) {
      GetDocument().SetSecureContextStateForTesting(
          SecureContextState::kSecure);
    }
  }

  void TestRegisterRejected(const String& script_url,
                            const String& scope,
                            const ScriptValueTest& value_test) {
    // When the registration is rejected, a register call must not reach
    // the provider.
    ServiceWorkerContainer* container =
        ServiceWorkerContainer::CreateForTesting(
            &GetDocument(),
            std::make_unique<NotReachedWebServiceWorkerProvider>());
    ScriptState::Scope script_scope(GetScriptState());
    RegistrationOptions* options = RegistrationOptions::Create();
    options->setScope(scope);
    ScriptPromise promise =
        container->registerServiceWorker(GetScriptState(), script_url, options);
    ExpectRejected(GetScriptState(), promise, value_test);
  }

  void TestGetRegistrationRejected(const String& document_url,
                                   const ScriptValueTest& value_test) {
    ServiceWorkerContainer* container =
        ServiceWorkerContainer::CreateForTesting(
            &GetDocument(),
            std::make_unique<NotReachedWebServiceWorkerProvider>());
    ScriptState::Scope script_scope(GetScriptState());
    ScriptPromise promise =
        container->getRegistration(GetScriptState(), document_url);
    ExpectRejected(GetScriptState(), promise, value_test);
  }
};

TEST_F(ServiceWorkerContainerTest, Register_CrossOriginScriptIsRejected) {
  SetPageURL("https://www.example.com");
  TestRegisterRejected(
      "https://www.example.com:8080/",  // Differs by port
      "https://www.example.com/",
      ExpectDOMException("SecurityError",
                         "Failed to register a ServiceWorker: The origin of "
                         "the provided scriptURL "
                         "('https://www.example.com:8080') does not match the "
                         "current origin ('https://www.example.com')."));
}

TEST_F(ServiceWorkerContainerTest, Register_UnsupportedSchemeIsRejected) {
  SetPageURL("https://www.example.com");
  TestRegisterRejected(
      "https://www.example.com",
      "wss://www.example.com/",  // Only support http and https
      ExpectTypeError(
          "Failed to register a ServiceWorker: The URL protocol "
          "of the scope ('wss://www.example.com/') is not supported."));
}

TEST_F(ServiceWorkerContainerTest, Register_CrossOriginScopeIsRejected) {
  SetPageURL("https://www.example.com");
  TestRegisterRejected(
      "https://www.example.com",
      "http://www.example.com/",  // Differs by protocol
      ExpectDOMException("SecurityError",
                         "Failed to register a ServiceWorker: The origin of "
                         "the provided scope ('http://www.example.com') does "
                         "not match the current origin "
                         "('https://www.example.com')."));
}

TEST_F(ServiceWorkerContainerTest, GetRegistration_CrossOriginURLIsRejected) {
  SetPageURL("https://www.example.com/");
  TestGetRegistrationRejected(
      "https://foo.example.com/",  // Differs by host
      ExpectDOMException("SecurityError",
                         "Failed to get a ServiceWorkerRegistration: The "
                         "origin of the provided documentURL "
                         "('https://foo.example.com') does not match the "
                         "current origin ('https://www.example.com')."));
}

class StubWebServiceWorkerProvider {
  DISALLOW_NEW();

 public:
  StubWebServiceWorkerProvider()
      : register_call_count_(0),
        get_registration_call_count_(0),
        script_type_(mojom::ScriptType::kClassic),
        update_via_cache_(mojom::ServiceWorkerUpdateViaCache::kImports) {}

  // Creates a WebServiceWorkerProvider. This can outlive the
  // StubWebServiceWorkerProvider, but |registerServiceWorker| and
  // other methods must not be called after the
  // StubWebServiceWorkerProvider dies.
  std::unique_ptr<WebServiceWorkerProvider> Provider() {
    return std::make_unique<WebServiceWorkerProviderImpl>(*this);
  }

  size_t RegisterCallCount() { return register_call_count_; }
  const WebURL& RegisterScope() { return register_scope_; }
  const WebURL& RegisterScriptURL() { return register_script_url_; }
  size_t GetRegistrationCallCount() { return get_registration_call_count_; }
  const WebURL& GetRegistrationURL() { return get_registration_url_; }
  mojom::ScriptType ScriptType() const { return script_type_; }
  mojom::ServiceWorkerUpdateViaCache UpdateViaCache() const {
    return update_via_cache_;
  }

 private:
  class WebServiceWorkerProviderImpl : public WebServiceWorkerProvider {
   public:
    WebServiceWorkerProviderImpl(StubWebServiceWorkerProvider& owner)
        : owner_(owner) {}

    ~WebServiceWorkerProviderImpl() override = default;

    void RegisterServiceWorker(
        const WebURL& scope,
        const WebURL& script_url,
        blink::mojom::ScriptType script_type,
        mojom::ServiceWorkerUpdateViaCache update_via_cache,
        const WebFetchClientSettingsObject& fetch_client_settings_object,
        std::unique_ptr<WebServiceWorkerRegistrationCallbacks> callbacks)
        override {
      owner_.register_call_count_++;
      owner_.register_scope_ = scope;
      owner_.register_script_url_ = script_url;
      owner_.script_type_ = script_type;
      owner_.update_via_cache_ = update_via_cache;
      registration_callbacks_to_delete_.push_back(std::move(callbacks));
    }

    void GetRegistration(
        const WebURL& document_url,
        std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks> callbacks)
        override {
      owner_.get_registration_call_count_++;
      owner_.get_registration_url_ = document_url;
      get_registration_callbacks_to_delete_.push_back(std::move(callbacks));
    }

    bool ValidateScopeAndScriptURL(const WebURL& scope,
                                   const WebURL& script_url,
                                   WebString* error_message) override {
      return true;
    }

   private:
    StubWebServiceWorkerProvider& owner_;
    Vector<std::unique_ptr<WebServiceWorkerRegistrationCallbacks>>
        registration_callbacks_to_delete_;
    Vector<std::unique_ptr<WebServiceWorkerGetRegistrationCallbacks>>
        get_registration_callbacks_to_delete_;
  };

 private:
  size_t register_call_count_;
  WebURL register_scope_;
  WebURL register_script_url_;
  size_t get_registration_call_count_;
  WebURL get_registration_url_;
  mojom::ScriptType script_type_;
  mojom::ServiceWorkerUpdateViaCache update_via_cache_;
};

TEST_F(ServiceWorkerContainerTest,
       RegisterUnregister_NonHttpsSecureOriginDelegatesToProvider) {
  SetPageURL("http://localhost/x/index.html");

  StubWebServiceWorkerProvider stub_provider;
  ServiceWorkerContainer* container = ServiceWorkerContainer::CreateForTesting(
      &GetDocument(), stub_provider.Provider());

  // register
  {
    ScriptState::Scope script_scope(GetScriptState());
    RegistrationOptions* options = RegistrationOptions::Create();
    options->setScope("y/");
    container->registerServiceWorker(GetScriptState(), "/x/y/worker.js",
                                     options);

    EXPECT_EQ(1ul, stub_provider.RegisterCallCount());
    EXPECT_EQ(WebURL(KURL("http://localhost/x/y/")),
              stub_provider.RegisterScope());
    EXPECT_EQ(WebURL(KURL("http://localhost/x/y/worker.js")),
              stub_provider.RegisterScriptURL());
    EXPECT_EQ(mojom::ScriptType::kClassic, stub_provider.ScriptType());
    EXPECT_EQ(mojom::ServiceWorkerUpdateViaCache::kImports,
              stub_provider.UpdateViaCache());
  }
}

TEST_F(ServiceWorkerContainerTest,
       GetRegistration_OmittedDocumentURLDefaultsToPageURL) {
  SetPageURL("http://localhost/x/index.html");

  StubWebServiceWorkerProvider stub_provider;
  ServiceWorkerContainer* container = ServiceWorkerContainer::CreateForTesting(
      &GetDocument(), stub_provider.Provider());

  {
    ScriptState::Scope script_scope(GetScriptState());
    container->getRegistration(GetScriptState(), "");
    EXPECT_EQ(1ul, stub_provider.GetRegistrationCallCount());
    EXPECT_EQ(WebURL(KURL("http://localhost/x/index.html")),
              stub_provider.GetRegistrationURL());
    EXPECT_EQ(mojom::ScriptType::kClassic, stub_provider.ScriptType());
    EXPECT_EQ(mojom::ServiceWorkerUpdateViaCache::kImports,
              stub_provider.UpdateViaCache());
  }
}

TEST_F(ServiceWorkerContainerTest,
       RegisterUnregister_UpdateViaCacheOptionDelegatesToProvider) {
  SetPageURL("http://localhost/x/index.html");

  StubWebServiceWorkerProvider stub_provider;
  ServiceWorkerContainer* container = ServiceWorkerContainer::CreateForTesting(
      &GetDocument(), stub_provider.Provider());

  // register
  {
    ScriptState::Scope script_scope(GetScriptState());
    RegistrationOptions* options = RegistrationOptions::Create();
    options->setUpdateViaCache("none");
    container->registerServiceWorker(GetScriptState(), "/x/y/worker.js",
                                     options);

    EXPECT_EQ(1ul, stub_provider.RegisterCallCount());
    EXPECT_EQ(WebURL(KURL(KURL(), "http://localhost/x/y/")),
              stub_provider.RegisterScope());
    EXPECT_EQ(WebURL(KURL(KURL(), "http://localhost/x/y/worker.js")),
              stub_provider.RegisterScriptURL());
    EXPECT_EQ(mojom::ScriptType::kClassic, stub_provider.ScriptType());
    EXPECT_EQ(mojom::ServiceWorkerUpdateViaCache::kNone,
              stub_provider.UpdateViaCache());
  }
}

TEST_F(ServiceWorkerContainerTest, Register_TypeOptionDelegatesToProvider) {
  SetPageURL("http://localhost/x/index.html");

  StubWebServiceWorkerProvider stub_provider;
  ServiceWorkerContainer* container = ServiceWorkerContainer::CreateForTesting(
      &GetDocument(), stub_provider.Provider());

  // register
  {
    ScriptState::Scope script_scope(GetScriptState());
    RegistrationOptions* options = RegistrationOptions::Create();
    options->setType("module");
    container->registerServiceWorker(GetScriptState(), "/x/y/worker.js",
                                     options);

    EXPECT_EQ(1ul, stub_provider.RegisterCallCount());
    EXPECT_EQ(WebURL(KURL(KURL(), "http://localhost/x/y/")),
              stub_provider.RegisterScope());
    EXPECT_EQ(WebURL(KURL(KURL(), "http://localhost/x/y/worker.js")),
              stub_provider.RegisterScriptURL());
    EXPECT_EQ(mojom::ScriptType::kModule, stub_provider.ScriptType());
    EXPECT_EQ(mojom::ServiceWorkerUpdateViaCache::kImports,
              stub_provider.UpdateViaCache());
  }
}

}  // namespace
}  // namespace blink
