// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/java/gin_java_method_invocation_helper.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/android/jni_android.h"
#include "base/values.h"
#include "content/common/android/gin_java_bridge_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class NullObjectDelegate
    : public GinJavaMethodInvocationHelper::ObjectDelegate {
 public:
  NullObjectDelegate() = default;

  NullObjectDelegate(const NullObjectDelegate&) = delete;
  NullObjectDelegate& operator=(const NullObjectDelegate&) = delete;

  ~NullObjectDelegate() override = default;

  base::android::ScopedJavaLocalRef<jobject> GetLocalRef(JNIEnv* env) override {
    return base::android::ScopedJavaLocalRef<jobject>();
  }

  base::android::ScopedJavaLocalRef<jclass> GetLocalClassRef(
      JNIEnv* env) override {
    return base::android::ScopedJavaLocalRef<jclass>();
  }

  const JavaMethod* FindMethod(const std::string& method_name,
                               size_t num_parameters) override {
    return nullptr;
  }

  bool IsObjectGetClassMethod(const JavaMethod* method) override {
    return false;
  }

  const base::android::JavaRef<jclass>& GetSafeAnnotationClass() override {
    return safe_annotation_class_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jclass> safe_annotation_class_;
};

class NullDispatcherDelegate
    : public GinJavaMethodInvocationHelper::DispatcherDelegate {
 public:
  NullDispatcherDelegate() = default;

  NullDispatcherDelegate(const NullDispatcherDelegate&) = delete;
  NullDispatcherDelegate& operator=(const NullDispatcherDelegate&) = delete;

  ~NullDispatcherDelegate() override = default;

  JavaObjectWeakGlobalRef GetObjectWeakRef(
      GinJavaBoundObject::ObjectID object_id) override {
    return JavaObjectWeakGlobalRef();
  }
};

}  // namespace

class GinJavaMethodInvocationHelperTest : public testing::Test {
};

namespace {

class CountingDispatcherDelegate
    : public GinJavaMethodInvocationHelper::DispatcherDelegate {
 public:
  CountingDispatcherDelegate() = default;

  CountingDispatcherDelegate(const CountingDispatcherDelegate&) = delete;
  CountingDispatcherDelegate& operator=(const CountingDispatcherDelegate&) =
      delete;

  ~CountingDispatcherDelegate() override = default;

  JavaObjectWeakGlobalRef GetObjectWeakRef(
      GinJavaBoundObject::ObjectID object_id) override {
    counters_[object_id]++;
    return JavaObjectWeakGlobalRef();
  }

  void AssertInvocationsCount(GinJavaBoundObject::ObjectID begin_object_id,
                              GinJavaBoundObject::ObjectID end_object_id) {
    EXPECT_EQ(end_object_id - begin_object_id,
              static_cast<int>(counters_.size()));
    for (GinJavaBoundObject::ObjectID i = begin_object_id;
         i < end_object_id; ++i) {
      EXPECT_LT(0, counters_[i]) << "ObjectID: " << i;
    }
  }

 private:
  typedef std::map<GinJavaBoundObject::ObjectID, int> Counters;
  Counters counters_;
};

}  // namespace

TEST_F(GinJavaMethodInvocationHelperTest, RetrievalOfObjectsNoObjects) {
  base::Value::List no_objects;
  for (int i = 0; i < 10; ++i) {
    no_objects.Append(i);
  }

  auto helper = base::MakeRefCounted<GinJavaMethodInvocationHelper>(
      std::make_unique<NullObjectDelegate>(), "foo", no_objects);
  CountingDispatcherDelegate counter;
  helper->Init(&counter);
  counter.AssertInvocationsCount(0, 0);
}

TEST_F(GinJavaMethodInvocationHelperTest, RetrievalOfObjectsHaveObjects) {
  base::Value::List objects;
  objects.Append(100);
  objects.Append(base::Value::FromUniquePtrValue(
      GinJavaBridgeValue::CreateObjectIDValue(1)));
  base::Value::List sub_list;
  sub_list.Append(200);
  sub_list.Append(base::Value::FromUniquePtrValue(
      GinJavaBridgeValue::CreateObjectIDValue(2)));
  objects.Append(std::move(sub_list));
  base::Value::Dict sub_dict;
  sub_dict.Set("1", 300);
  sub_dict.Set("2", base::Value::FromUniquePtrValue(
                        GinJavaBridgeValue::CreateObjectIDValue(3)));
  objects.Append(std::move(sub_dict));
  base::Value::List sub_list_with_dict;
  base::Value::Dict sub_sub_dict;
  sub_sub_dict.Set("1", base::Value::FromUniquePtrValue(
                            GinJavaBridgeValue::CreateObjectIDValue(4)));
  sub_list_with_dict.Append(std::move(sub_sub_dict));
  objects.Append(std::move(sub_list_with_dict));
  base::Value::Dict sub_dict_with_list;
  base::Value::List sub_sub_list;
  sub_sub_list.Append(base::Value::FromUniquePtrValue(
      GinJavaBridgeValue::CreateObjectIDValue(5)));
  sub_dict_with_list.Set("1", std::move(sub_sub_list));
  objects.Append(std::move(sub_dict_with_list));

  auto helper = base::MakeRefCounted<GinJavaMethodInvocationHelper>(
      std::make_unique<NullObjectDelegate>(), "foo", objects);
  CountingDispatcherDelegate counter;
  helper->Init(&counter);
  counter.AssertInvocationsCount(1, 6);
}

namespace {

class ObjectIsGoneObjectDelegate : public NullObjectDelegate {
 public:
  ObjectIsGoneObjectDelegate() :
      get_local_ref_called_(false) {
    // We need a Java Method object to create a valid JavaMethod instance.
    JNIEnv* env = base::android::AttachCurrentThread();
    base::android::ScopedJavaLocalRef<jclass> clazz(
        base::android::GetClass(env, "java/lang/Object"));
    jmethodID method_id =
        base::android::MethodID::Get<base::android::MethodID::TYPE_INSTANCE>(
            env, clazz.obj(), "hashCode", "()I");
    EXPECT_TRUE(method_id);
    base::android::ScopedJavaLocalRef<jobject> method_obj(
        env, env->ToReflectedMethod(clazz.obj(), method_id, false));
    EXPECT_TRUE(method_obj.obj());
    method_ = std::make_unique<JavaMethod>(method_obj);
  }

  ObjectIsGoneObjectDelegate(const ObjectIsGoneObjectDelegate&) = delete;
  ObjectIsGoneObjectDelegate& operator=(const ObjectIsGoneObjectDelegate&) =
      delete;

  ~ObjectIsGoneObjectDelegate() override = default;

  base::android::ScopedJavaLocalRef<jobject> GetLocalRef(JNIEnv* env) override {
    get_local_ref_called_ = true;
    return NullObjectDelegate::GetLocalRef(env);
  }

  const JavaMethod* FindMethod(const std::string& method_name,
                               size_t num_parameters) override {
    return method_.get();
  }

  bool get_local_ref_called() { return get_local_ref_called_; }

  const std::string& get_method_name() { return method_->name(); }

 protected:
  std::unique_ptr<JavaMethod> method_;
  bool get_local_ref_called_;
};

}  // namespace

TEST_F(GinJavaMethodInvocationHelperTest, HandleObjectIsGone) {
  base::Value::List no_objects;
  auto object_delegate_unique = std::make_unique<ObjectIsGoneObjectDelegate>();
  ObjectIsGoneObjectDelegate* object_delegate = object_delegate_unique.get();
  auto helper = base::MakeRefCounted<GinJavaMethodInvocationHelper>(
      std::move(object_delegate_unique), object_delegate->get_method_name(),
      no_objects);
  NullDispatcherDelegate dispatcher;
  helper->Init(&dispatcher);
  EXPECT_FALSE(object_delegate->get_local_ref_called());
  EXPECT_EQ(mojom::GinJavaBridgeError::kGinJavaBridgeNoError,
            helper->GetInvocationError());
  helper->Invoke();
  EXPECT_TRUE(object_delegate->get_local_ref_called());
  EXPECT_TRUE(helper->HoldsPrimitiveResult());
  EXPECT_TRUE(helper->GetPrimitiveResult().empty());
  EXPECT_EQ(mojom::GinJavaBridgeError::kGinJavaBridgeObjectIsGone,
            helper->GetInvocationError());
}

namespace {

class MethodNotFoundObjectDelegate : public NullObjectDelegate {
 public:
  MethodNotFoundObjectDelegate() : find_method_called_(false) {}

  MethodNotFoundObjectDelegate(const MethodNotFoundObjectDelegate&) = delete;
  MethodNotFoundObjectDelegate& operator=(const MethodNotFoundObjectDelegate&) =
      delete;

  ~MethodNotFoundObjectDelegate() override = default;

  base::android::ScopedJavaLocalRef<jobject> GetLocalRef(JNIEnv* env) override {
    return base::android::ScopedJavaLocalRef<jobject>(
        env, static_cast<jobject>(env->FindClass("java/lang/String")));
  }

  const JavaMethod* FindMethod(const std::string& method_name,
                               size_t num_parameters) override {
    find_method_called_ = true;
    return nullptr;
  }

  bool find_method_called() const { return find_method_called_; }

 protected:
  bool find_method_called_;
};

}  // namespace

TEST_F(GinJavaMethodInvocationHelperTest, HandleMethodNotFound) {
  base::Value::List no_objects;
  auto object_delegate_unique =
      std::make_unique<MethodNotFoundObjectDelegate>();
  MethodNotFoundObjectDelegate* object_delegate = object_delegate_unique.get();
  auto helper = base::MakeRefCounted<GinJavaMethodInvocationHelper>(
      std::move(object_delegate_unique), "foo", no_objects);
  NullDispatcherDelegate dispatcher;
  helper->Init(&dispatcher);
  EXPECT_FALSE(object_delegate->find_method_called());
  EXPECT_EQ(mojom::GinJavaBridgeError::kGinJavaBridgeNoError,
            helper->GetInvocationError());
  helper->Invoke();
  EXPECT_TRUE(object_delegate->find_method_called());
  EXPECT_TRUE(helper->HoldsPrimitiveResult());
  EXPECT_TRUE(helper->GetPrimitiveResult().empty());
  EXPECT_EQ(mojom::GinJavaBridgeError::kGinJavaBridgeMethodNotFound,
            helper->GetInvocationError());
}

namespace {

class GetClassObjectDelegate : public MethodNotFoundObjectDelegate {
 public:
  GetClassObjectDelegate() : get_class_called_(false) {}

  GetClassObjectDelegate(const GetClassObjectDelegate&) = delete;
  GetClassObjectDelegate& operator=(const GetClassObjectDelegate&) = delete;

  ~GetClassObjectDelegate() override = default;

  const JavaMethod* FindMethod(const std::string& method_name,
                               size_t num_parameters) override {
    find_method_called_ = true;
    return kFakeGetClass;
  }

  bool IsObjectGetClassMethod(const JavaMethod* method) override {
    get_class_called_ = true;
    return kFakeGetClass == method;
  }

  bool get_class_called() const { return get_class_called_; }

 private:
  static const JavaMethod* kFakeGetClass;
  bool get_class_called_;
};

// We don't expect GinJavaMethodInvocationHelper to actually invoke the
// method, since the point of the test is to verify whether calls to
// 'getClass' get blocked.
const JavaMethod* GetClassObjectDelegate::kFakeGetClass =
    (JavaMethod*)0xdeadbeef;

}  // namespace

TEST_F(GinJavaMethodInvocationHelperTest, HandleGetClassInvocation) {
  base::Value::List no_objects;
  auto object_delegate_unique = std::make_unique<GetClassObjectDelegate>();
  GetClassObjectDelegate* object_delegate = object_delegate_unique.get();
  auto helper = base::MakeRefCounted<GinJavaMethodInvocationHelper>(
      std::move(object_delegate_unique), "foo", no_objects);
  NullDispatcherDelegate dispatcher;
  helper->Init(&dispatcher);
  EXPECT_FALSE(object_delegate->find_method_called());
  EXPECT_FALSE(object_delegate->get_class_called());
  EXPECT_EQ(mojom::GinJavaBridgeError::kGinJavaBridgeNoError,
            helper->GetInvocationError());
  helper->Invoke();
  EXPECT_TRUE(object_delegate->find_method_called());
  EXPECT_TRUE(object_delegate->get_class_called());
  EXPECT_TRUE(helper->HoldsPrimitiveResult());
  EXPECT_TRUE(helper->GetPrimitiveResult().empty());
  EXPECT_EQ(
      mojom::GinJavaBridgeError::kGinJavaBridgeAccessToObjectGetClassIsBlocked,
      helper->GetInvocationError());
}

}  // namespace content
