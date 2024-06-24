// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/tracing_controller_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/tracing/tracing_controller_impl.h"
#include "content/public/browser/tracing_controller.h"
#include "services/tracing/public/cpp/perfetto/perfetto_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_session.h"
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/common/trace_stats.gen.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_main_dex_jni/TracingControllerAndroidImpl_jni.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ScopedJavaGlobalRef;

namespace content {
namespace {

// Currently active tracing session.
perfetto::TracingSession* g_tracing_session = nullptr;

void ReadProtobufTraceData(
    scoped_refptr<TracingController::TraceDataEndpoint> endpoint,
    perfetto::TracingSession::ReadTraceCallbackArgs args) {
  if (args.size) {
    auto data_string = std::make_unique<std::string>(args.data, args.size);
    endpoint->ReceiveTraceChunk(std::move(data_string));
  }
  if (!args.has_more)
    endpoint->ReceivedTraceFinalContents();
}

void ReadJsonTraceData(
    scoped_refptr<TracingController::TraceDataEndpoint> endpoint,
    tracing::TracePacketTokenizer& tokenizer,
    perfetto::TracingSession::ReadTraceCallbackArgs args) {
  if (args.size) {
    auto packets =
        tokenizer.Parse(reinterpret_cast<const uint8_t*>(args.data), args.size);
    for (const auto& packet : packets) {
      for (const auto& slice : packet.slices()) {
        auto data_string = std::make_unique<std::string>(
            reinterpret_cast<const char*>(slice.start), slice.size);
        endpoint->ReceiveTraceChunk(std::move(data_string));
      }
    }
  }
  if (!args.has_more) {
    DCHECK(!tokenizer.has_more());
    endpoint->ReceivedTraceFinalContents();
  }
}

}  // namespace

static jlong JNI_TracingControllerAndroidImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  TracingControllerAndroid* profiler = new TracingControllerAndroid(env, obj);
  return reinterpret_cast<intptr_t>(profiler);
}

TracingControllerAndroid::TracingControllerAndroid(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj)
    : weak_java_object_(env, obj) {}

TracingControllerAndroid::~TracingControllerAndroid() {}

void TracingControllerAndroid::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

bool TracingControllerAndroid::StartTracing(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jcategories,
    const JavaParamRef<jstring>& jtraceoptions,
    bool use_protobuf) {
  std::string categories =
      base::android::ConvertJavaStringToUTF8(env, jcategories);
  std::string options =
      base::android::ConvertJavaStringToUTF8(env, jtraceoptions);

  // This log is required by adb_profile_chrome.py.
  LOG(WARNING) << "Logging performance trace to file";

  base::trace_event::TraceConfig trace_config(categories, options);
  perfetto::TraceConfig perfetto_config = tracing::GetDefaultPerfettoConfig(
      base::trace_event::TraceConfig(), /*privacy_filtering_enabled=*/false,
      /*convert_to_legacy_json=*/!use_protobuf);
  delete g_tracing_session;
  g_tracing_session =
      perfetto::Tracing::NewTrace(perfetto::BackendType::kCustomBackend)
          .release();
  g_tracing_session->Setup(perfetto_config);
  g_tracing_session->Start();
  return true;
}

void TracingControllerAndroid::StopTracing(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jfilepath,
    bool compress_file,
    bool use_protobuf,
    const base::android::JavaParamRef<jobject>& callback) {
  base::FilePath file_path(
      base::android::ConvertJavaStringToUTF8(env, jfilepath));
  ScopedJavaGlobalRef<jobject> global_callback(env, callback);
  auto endpoint = TracingController::CreateFileEndpoint(
      file_path, base::BindOnce(&TracingControllerAndroid::OnTracingStopped,
                                weak_factory_.GetWeakPtr(), global_callback));

  if (!g_tracing_session) {
    LOG(ERROR) << "Tried to stop a non-existent tracing session";
    OnTracingStopped(global_callback);
    return;
  }

  if (compress_file) {
    endpoint = TracingControllerImpl::CreateCompressedStringEndpoint(
        endpoint, /*compress_with_background_priority=*/true);
  }

  auto session = base::MakeRefCounted<
      base::RefCountedData<std::unique_ptr<perfetto::TracingSession>>>(
      base::WrapUnique(g_tracing_session));
  g_tracing_session = nullptr;
  if (use_protobuf) {
    session->data->SetOnStopCallback([session, endpoint] {
      session->data->ReadTrace(
          [session,
           endpoint](perfetto::TracingSession::ReadTraceCallbackArgs args) {
            ReadProtobufTraceData(endpoint, args);
          });
    });
  } else {
    auto tokenizer = base::MakeRefCounted<
        base::RefCountedData<std::unique_ptr<tracing::TracePacketTokenizer>>>(
        std::make_unique<tracing::TracePacketTokenizer>());
    session->data->SetOnStopCallback([session, tokenizer, endpoint] {
      session->data->ReadTrace(
          [session, tokenizer,
           endpoint](perfetto::TracingSession::ReadTraceCallbackArgs args) {
            ReadJsonTraceData(endpoint, *tokenizer->data, args);
          });
    });
  }
  session->data->Stop();
}

base::FilePath TracingControllerAndroid::GenerateTracingFilePath(
    const std::string& basename) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jfilename =
      Java_TracingControllerAndroidImpl_generateTracingFilePath(
          env, base::android::ConvertUTF8ToJavaString(env, basename));
  return base::FilePath(
      base::android::ConvertJavaStringToUTF8(env, jfilename.obj()));
}

void TracingControllerAndroid::OnTracingStopped(
    const base::android::ScopedJavaGlobalRef<jobject>& callback) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object_.get(env);
  if (obj.obj())
    Java_TracingControllerAndroidImpl_onTracingStopped(env, obj, callback);
}

bool TracingControllerAndroid::GetKnownCategoriesAsync(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback) {
  ScopedJavaGlobalRef<jobject> global_callback(env, callback);
  // TODO(skyostil): Get the categories from Perfetto instead.
  return TracingController::GetInstance()->GetCategories(
      base::BindOnce(&TracingControllerAndroid::OnKnownCategoriesReceived,
                     weak_factory_.GetWeakPtr(), global_callback));
}

void TracingControllerAndroid::OnKnownCategoriesReceived(
    const ScopedJavaGlobalRef<jobject>& callback,
    const std::set<std::string>& categories_received) {
  base::Value::List category_list;
  for (const std::string& category : categories_received)
    category_list.Append(category);
  std::string received_category_list;
  base::JSONWriter::Write(base::Value(std::move(category_list)),
                          &received_category_list);

  // This log is required by adb_profile_chrome.py.
  // TODO(crbug.com/40092856): Replace (users of) this with DevTools' Tracing
  // API.
  LOG(WARNING) << "{\"traceCategoriesList\": " << received_category_list << "}";

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object_.get(env);
  if (obj.obj()) {
    std::vector<std::string> category_vector(categories_received.begin(),
                                             categories_received.end());
    base::android::ScopedJavaLocalRef<jobjectArray> jcategories =
        base::android::ToJavaArrayOfStrings(env, category_vector);
    Java_TracingControllerAndroidImpl_onKnownCategoriesReceived(
        env, obj, jcategories, callback);
  }
}

static ScopedJavaLocalRef<jstring>
JNI_TracingControllerAndroidImpl_GetDefaultCategories(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  base::trace_event::TraceConfig trace_config;
  return base::android::ConvertUTF8ToJavaString(
      env, trace_config.ToCategoryFilterString());
}

bool TracingControllerAndroid::GetTraceBufferUsageAsync(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback) {
  ScopedJavaGlobalRef<jobject> global_callback(env, callback);
  auto weak_callback =
      base::BindOnce(&TracingControllerAndroid::OnTraceBufferUsageReceived,
                     weak_factory_.GetWeakPtr(), global_callback);

  if (!g_tracing_session) {
    std::move(weak_callback)
        .Run(/*percent_full=*/0.f, /*approximate_event_count=*/0);
    return true;
  }

  // |weak_callback| is move-only, so in order to pass it through a copied
  // lambda we need to temporarily move it on the heap.
  auto shared_callback = base::MakeRefCounted<
      base::RefCountedData<base::OnceCallback<void(float, size_t)>>>(
      std::move(weak_callback));
  g_tracing_session->GetTraceStats(
      [shared_callback](
          perfetto::TracingSession::GetTraceStatsCallbackArgs args) {
        float percent_full = 0;
        perfetto::protos::gen::TraceStats trace_stats;
        if (args.success &&
            trace_stats.ParseFromArray(args.trace_stats_data.data(),
                                       args.trace_stats_data.size())) {
          percent_full = tracing::GetTraceBufferUsage(trace_stats);
        }
        // TODO(skyostil): Remove approximate_event_count since no-one is using
        // it.
        std::move(shared_callback->data)
            .Run(percent_full, /*approximate_event_count=*/0);
      });
  return true;
}

void TracingControllerAndroid::OnTraceBufferUsageReceived(
    const ScopedJavaGlobalRef<jobject>& callback,
    float percent_full,
    size_t approximate_event_count) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> obj = weak_java_object_.get(env);
  if (obj.obj()) {
    Java_TracingControllerAndroidImpl_onTraceBufferUsageReceived(
        env, obj, percent_full, approximate_event_count, callback);
  }
}

}  // namespace content
