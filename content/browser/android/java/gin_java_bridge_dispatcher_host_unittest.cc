// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/run_loop.h"
#include "base/test/run_until.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "components/origin_matcher/origin_matcher.h"
#include "content/browser/android/java/java_bridge_thread.h"
#include "content/common/gin_java_bridge.mojom.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/scoped_interface_endpoint_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

namespace content {
namespace {

class FakeGinJavaBridge : public mojom::GinJavaBridge {
 public:
  FakeGinJavaBridge() = default;
  ~FakeGinJavaBridge() override = default;

  void BindReceiver(mojo::ScopedInterfaceEndpointHandle handle) {
    receivers_.Add(this, mojo::PendingAssociatedReceiver<mojom::GinJavaBridge>(
                             std::move(handle)));
  }

  // mojom::GinJavaBridge:
  void AddNamedObject(const std::string& name,
                      int32_t object_id,
                      const origin_matcher::OriginMatcher& matcher) override {
    named_objects_[name] = object_id;
  }

  void RemoveNamedObject(const std::string& name) override {
    named_objects_.erase(name);
  }

  void SetHost(mojo::PendingRemote<mojom::GinJavaBridgeHost> host) override {
    host_remote_.reset();
    host_remote_.Bind(std::move(host));
  }

  mojom::GinJavaBridgeHost* host() { return host_remote_.get(); }

  void Flush() {
    EXPECT_TRUE(base::test::RunUntil([this]() { return !receivers_.empty(); }));
    receivers_.FlushForTesting();
    FlushJavaBridgeThread();
    if (host_remote_) {
      host_remote_.FlushForTesting();
    }
  }

  static void FlushJavaBridgeThread() {
    base::RunLoop run_loop;
    JavaBridgeThread::GetTaskRunner()->PostTaskAndReply(
        FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
    run_loop.Run();
  }

  std::optional<int32_t> GetObjectId(const std::string& name) const {
    auto it = named_objects_.find(name);
    if (it == named_objects_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

 private:
  mojo::AssociatedReceiverSet<mojom::GinJavaBridge> receivers_;
  mojo::Remote<mojom::GinJavaBridgeHost> host_remote_;
  std::map<std::string, int32_t> named_objects_;
};

origin_matcher::OriginMatcher CreateMatcher(
    const std::vector<std::string>& rules) {
  origin_matcher::OriginMatcher matcher;
  for (const std::string& rule : rules) {
    CHECK(matcher.AddRuleFromString(rule));
  }
  return matcher;
}

base::android::ScopedJavaLocalRef<jobject> CreateJavaHashSet(JNIEnv* env) {
  base::android::ScopedJavaLocalRef<jclass> clazz =
      base::android::GetClass(env, "java/util/HashSet");
  jmethodID ctor =
      base::android::MethodID::Get<base::android::MethodID::TYPE_INSTANCE>(
          env, clazz.obj(), "<init>", "()V");
  return jni_zero::AdoptRef(env, env->NewObject(clazz.obj(), ctor));
}

}  // namespace

class GinJavaBridgeDispatcherHostTest : public RenderViewHostImplTestHarness {
 protected:
  void TearDown() override {
    DeleteContents();
    FakeGinJavaBridge::FlushJavaBridgeThread();
    dispatcher_host_.reset();
    bridges_.clear();
    RenderViewHostImplTestHarness::TearDown();
  }

  GinJavaBridgeDispatcherHost* CreateDispatcherHost() {
    JNIEnv* env = base::android::AttachCurrentThread();
    retained_set_.Reset(CreateJavaHashSet(env));
    dispatcher_host_ = base::MakeRefCounted<GinJavaBridgeDispatcherHost>(
        contents(), retained_set_);
    return dispatcher_host_.get();
  }

  FakeGinJavaBridge* CreateBridgeForFrame(RenderFrameHost* rfh) {
    auto bridge = std::make_unique<FakeGinJavaBridge>();
    FakeGinJavaBridge* raw_bridge = bridge.get();
    rfh->GetRemoteAssociatedInterfaces()->OverrideBinderForTesting(
        mojom::GinJavaBridge::Name_,
        base::BindRepeating(&FakeGinJavaBridge::BindReceiver,
                            base::Unretained(raw_bridge)));
    bridges_.push_back(std::move(bridge));
    return raw_bridge;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> retained_set_;
  scoped_refptr<GinJavaBridgeDispatcherHost> dispatcher_host_;
  std::vector<std::unique_ptr<FakeGinJavaBridge>> bridges_;
};

TEST_F(GinJavaBridgeDispatcherHostTest,
       AllowedOriginCanGetObjectAndInvokeMethod) {
  NavigateAndCommit(GURL("https://trusted.example/index.html"));

  FakeGinJavaBridge* bridge = CreateBridgeForFrame(main_rfh());

  GinJavaBridgeDispatcherHost* host = CreateDispatcherHost();
  JNIEnv* env = base::android::AttachCurrentThread();
  auto java_string = base::android::ConvertUTF8ToJavaString(env, "secret");
  host->AddNamedObject("AuthBridge", java_string,
                       base::android::ScopedJavaLocalRef<jclass>(),
                       CreateMatcher({"https://trusted.example"}));
  bridge->Flush();

  std::optional<int32_t> object_id = bridge->GetObjectId("AuthBridge");
  ASSERT_TRUE(object_id.has_value());
  ASSERT_NE(nullptr, bridge->host());

  mojo::Remote<mojom::GinJavaBridgeRemoteObject> remote_object;
  bridge->host()->GetObject(*object_id,
                            remote_object.BindNewPipeAndPassReceiver());

  base::test::TestFuture<bool> has_method_future;
  remote_object->HasMethod("toString", has_method_future.GetCallback());
  EXPECT_TRUE(has_method_future.Get());

  base::test::TestFuture<mojom::GinJavaBridgeError, base::ListValue>
      invoke_future;
  remote_object->InvokeMethod("toString", base::ListValue(),
                              invoke_future.GetCallback());
  EXPECT_EQ(mojom::GinJavaBridgeError::kGinJavaBridgeNoError,
            invoke_future.Get<0>());
  ASSERT_EQ(1u, invoke_future.Get<1>().size());
  EXPECT_EQ("secret", invoke_future.Get<1>()[0].GetString());
}

}  // namespace content
