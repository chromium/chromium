// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/html_install_element.h"

#include "base/containers/span.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom-blink.h"
#include "third_party/blink/public/strings/grit/permission_element_strings.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_init.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_permission_element_test_helper.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionObserver;
using mojom::blink::PermissionService;
using MojoPermissionStatus = mojom::blink::PermissionStatus;

namespace {

constexpr char kInstallString[] = "Install";
constexpr char kExampleSite[] = "https://site.example/app.manifest";

String ResourceIdToString(int resource_id) {
  switch (resource_id) {
    case IDS_PERMISSION_REQUEST_INSTALL:
      return kInstallString;
    default:
      return "";
  }
}

class LocalePlatformSupport : public TestingPlatformSupport {
 public:
  WebString QueryLocalizedString(int resource_id) override {
    if (ResourceIdToString(resource_id).empty()) {
      return TestingPlatformSupport::QueryLocalizedString(resource_id);
    }
    return ResourceIdToString(resource_id);
  }
};

class MockWebInstallService : public mojom::blink::WebInstallService {
 public:
  MockWebInstallService() = default;
  ~MockWebInstallService() override = default;

  void BindHandle(mojo::ScopedMessagePipeHandle handle) {
    receivers_.Add(this, mojo::PendingReceiver<mojom::blink::WebInstallService>(
                             std::move(handle)));
  }

  // mojom::blink::WebInstallService impl:
  void Install(mojom::blink::InstallOptionsPtr options,
               InstallCallback callback) override {
    // Only for installs from the JS API. Use InstallFromElement() instead.
    NOTIMPLEMENTED();
  }

  void InstallFromElement(mojom::blink::InstallOptionsPtr options,
                          InstallCallback callback) override {
    CHECK(!callback_) << "Keep the tests simple: one call at a time.";
    options_ = std::move(options);
    callback_ = std::move(callback);
    called_.SetValue();
  }

  // Test helpers:
  void WaitForCall() { EXPECT_TRUE(called_.Wait()); }

  void RespondWithSuccess() {
    CHECK(callback_);
    std::move(callback_).Run(mojom::blink::WebInstallServiceResult::kSuccess,
                             KURL());
    called_.Clear();
  }

  void RespondWithAbortError() {
    CHECK(callback_);
    std::move(callback_).Run(mojom::blink::WebInstallServiceResult::kAbortError,
                             KURL());
    called_.Clear();
  }

  const mojom::blink::InstallOptionsPtr& options() const { return options_; }

 private:
  mojo::ReceiverSet<mojom::blink::WebInstallService> receivers_;
  mojom::blink::InstallOptionsPtr options_;
  InstallCallback callback_;
  base::test::TestFuture<void> called_;
};

}  // namespace

class HTMLInstallElementTestBase : public PageTestBase {
 protected:
  explicit HTMLInstallElementTestBase()
      : PageTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void WaitForElementRegistration(HTMLInstallElement* element) {
    GetDocument().body()->AppendChild(element);
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    WaitForPermissionElementRegistration(element);
  }

  void CheckInnerText(HTMLInstallElement* element,
                      const String& expected_text) {
    GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kTest);
    GetDocument().View()->UpdateAllLifecyclePhasesForTest();
    base::RunLoop run_loop;
    // `UpdateText` is called via `PostTask`, so `innerText` is checked within a
    // `PostTask` to ensure it's updated.
    GetDocument()
        .GetTaskRunner(TaskType::kInternalDefault)
        ->PostTask(
            FROM_HERE,
            blink::BindOnce(
                [](HTMLInstallElement* element, const String& expected_text,
                   base::RepeatingClosure quit_closure) {
                  EXPECT_EQ(
                      expected_text,
                      element->permission_text_span_for_testing()->innerText());
                  quit_closure.Run();
                },
                WeakPersistent(element), expected_text,
                run_loop.QuitClosure()));
    run_loop.Run();
  }

  void SetUp() override {
    PageTestBase::SetUp();
    scoped_feature_list_.InitAndEnableFeature(blink::features::kInstallElement);
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_,
        blink::BindRepeating(
            &PermissionElementTestPermissionService::BindHandle,
            base::Unretained(&permission_service_)));
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::WebInstallService::Name_,
        blink::BindRepeating(&MockWebInstallService::BindHandle,
                             base::Unretained(&web_install_service_)));
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        PermissionService::Name_, {});
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::WebInstallService::Name_, {});
    PageTestBase::TearDown();
  }

 protected:
  MockWebInstallService web_install_service_;

 private:
  PermissionElementTestPermissionService permission_service_;
  ScopedTestingPlatformSupport<LocalePlatformSupport> support_;
  base::test::ScopedFeatureList scoped_feature_list_;
  ScopedInstallElementForTest scoped_feature_{true};
  // We need this to test activation behavior in the unit testing framework
  // without doing a thrilling amount of setup.
  ScopedBypassPepcSecurityForTestingForTest bypass_pepc_security_for_testing_{
      true};
};

TEST_F(HTMLInstallElementTestBase, Type) {
  HTMLInstallElement* element =
      MakeGarbageCollected<HTMLInstallElement>(GetDocument());
  EXPECT_EQ(AtomicString("install"), element->GetType());
}

TEST_F(HTMLInstallElementTestBase, InstallUrl) {
  HTMLInstallElement* element =
      MakeGarbageCollected<HTMLInstallElement>(GetDocument());
  EXPECT_TRUE(element->InstallUrl().empty());

  element->setAttribute(html_names::kInstallurlAttr,
                        AtomicString(kExampleSite));
  EXPECT_EQ(kExampleSite, element->InstallUrl());
}

TEST_F(HTMLInstallElementTestBase, ManifestId) {
  HTMLInstallElement* element =
      MakeGarbageCollected<HTMLInstallElement>(GetDocument());
  EXPECT_TRUE(element->ManifestId().empty());

  constexpr char kManifestId[] = "https://site.example/manifest.json";
  element->setAttribute(html_names::kManifestidAttr, AtomicString(kManifestId));
  EXPECT_EQ(kManifestId, element->ManifestId());
}

TEST_F(HTMLInstallElementTestBase, RenderedText) {
  {
    HTMLInstallElement* element =
        MakeGarbageCollected<HTMLInstallElement>(GetDocument());

    WaitForElementRegistration(element);

    CheckInnerText(element, "Install");
  }

  {
    HTMLInstallElement* element =
        MakeGarbageCollected<HTMLInstallElement>(GetDocument());

    element->setAttribute(html_names::kInstallurlAttr,
                          AtomicString(kExampleSite));

    WaitForElementRegistration(element);

    // TODO(crbug.com/467103133): Update when site-specific information is
    // rendered.
    CheckInnerText(element, kInstallString);
  }
}

TEST_F(HTMLInstallElementTestBase, ActivationSuccess) {
  HTMLInstallElement* element =
      MakeGarbageCollected<HTMLInstallElement>(GetDocument());
  WaitForElementRegistration(element);

  element->DispatchEvent(*Event::Create(event_type_names::kDOMActivate));

  // The `Install` method should be called.
  web_install_service_.WaitForCall();

  // No `installurl` was specified, so no options were sent to the service.
  EXPECT_TRUE(web_install_service_.options().is_null());

  // Success should trigger a `promptaction` event.
  web_install_service_.RespondWithSuccess();
  MakeGarbageCollected<WaitForEvent>(element, event_type_names::kPromptaction);
}

TEST_F(HTMLInstallElementTestBase, ActivationSuccessWithInstallUrl) {
  // Create the element with installurl only.
  HTMLInstallElement* element =
      MakeGarbageCollected<HTMLInstallElement>(GetDocument());
  element->setAttribute(html_names::kInstallurlAttr,
                        AtomicString(kExampleSite));
  WaitForElementRegistration(element);

  element->DispatchEvent(*Event::Create(event_type_names::kDOMActivate));

  // The `Install` method should be called.
  web_install_service_.WaitForCall();

  // `installurl` was specified, so options were sent to the service.
  EXPECT_FALSE(web_install_service_.options().is_null());
  EXPECT_EQ(web_install_service_.options()->install_url, KURL(kExampleSite));

  // Success should trigger a `promptaction` event.
  web_install_service_.RespondWithSuccess();
  MakeGarbageCollected<WaitForEvent>(element, event_type_names::kPromptaction);
}

TEST_F(HTMLInstallElementTestBase,
       ActivationSuccessWithInstallUrlAndManifestId) {
  // Create the element with installurl and manifestid.
  HTMLInstallElement* element =
      MakeGarbageCollected<HTMLInstallElement>(GetDocument());
  element->setAttribute(html_names::kInstallurlAttr,
                        AtomicString(kExampleSite));
  constexpr char kManifestId[] = "https://site.example/manifest.json";
  element->setAttribute(html_names::kManifestidAttr, AtomicString(kManifestId));
  WaitForElementRegistration(element);

  element->DispatchEvent(*Event::Create(event_type_names::kDOMActivate));

  // The `Install` method should be called.
  web_install_service_.WaitForCall();

  // Both `installurl` and `manifestid` were specified.
  EXPECT_FALSE(web_install_service_.options().is_null());
  EXPECT_EQ(web_install_service_.options()->install_url, KURL(kExampleSite));
  EXPECT_EQ(web_install_service_.options()->manifest_id, KURL(kManifestId));

  // Success should trigger a `promptaction` event.
  web_install_service_.RespondWithSuccess();
  MakeGarbageCollected<WaitForEvent>(element, event_type_names::kPromptaction);
}

TEST_F(HTMLInstallElementTestBase, ActivationAborted) {
  HTMLInstallElement* element =
      MakeGarbageCollected<HTMLInstallElement>(GetDocument());
  WaitForElementRegistration(element);

  element->DispatchEvent(*Event::Create(event_type_names::kDOMActivate));

  // The `Install` method should be called.
  web_install_service_.WaitForCall();

  // No `installurl` was specified, so no options were sent to the service.
  EXPECT_TRUE(web_install_service_.options().is_null());

  // AbortError should trigger a `promptdismiss` event.
  web_install_service_.RespondWithAbortError();
  MakeGarbageCollected<WaitForEvent>(element, event_type_names::kPromptdismiss);
}

}  // namespace blink
