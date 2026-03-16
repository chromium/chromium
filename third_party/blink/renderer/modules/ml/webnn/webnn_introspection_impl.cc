// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/webnn_introspection_impl.h"

#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/static_constructors.h"

namespace blink {

// static
WebNNIntrospectionImpl& WebNNIntrospectionImpl::GetInstance() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(WebNNIntrospectionImpl, instance, ());
  return instance;
}

WebNNIntrospectionImpl::WebNNIntrospectionImpl() = default;

// static
void WebNNIntrospectionImpl::BindReceiver(
    mojo::PendingReceiver<blink::mojom::blink::WebNNIntrospection> receiver) {
  auto& instance = GetInstance();
  // When the browser disables graph recording, it drops its remote. When it
  // (re-)enables it, the browser re-binds the connection. To handle rapid
  // toggling or other edge cases, we unconditionally reset to overwrite any
  // still-existing receiver.
  instance.receiver_.reset();
  instance.receiver_.Bind(std::move(receiver));
}

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
bool WebNNIntrospectionImpl::IsGraphRecordingEnabled() const {
  return is_graph_recording_enabled_.load();
}

void WebNNIntrospectionImpl::SetClient(
    ::mojo::PendingRemote<blink::mojom::blink::WebNNIntrospectionClient>
        client) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // When the browser disables graph recording, it drops its remote. When it
  // (re-)enables it, the browser re-binds the connection. To handle rapid
  // toggling or other edge cases, we unconditionally reset to overwrite any
  // still-existing remote.
  client_.reset();
  client_.Bind(std::move(client));
  // Safe because `WebNNIntrospectionImpl` is a singleton and never destroyed.
  client_.set_disconnect_handler(base::BindOnce([]() {
    WebNNIntrospectionImpl::GetInstance().is_graph_recording_enabled_.store(
        false);
  }));
  is_graph_recording_enabled_.store(true);
}

void WebNNIntrospectionImpl::OnGraphRecorded(
    ::mojo_base::BigBuffer json_data) const {
  auto task_runner =
      Thread::MainThread()->GetTaskRunner(MainThreadTaskRunnerRestricted());
  if (!task_runner->RunsTasksInCurrentSequence()) {
    // WebNN graph recording can be triggered on worker threads, but the
    // `client_` is bound on the main thread. Post a task to the main thread to
    // ensure the callback is run on the same sequence as `client_`.
    // Safe because `WebNNIntrospectionImpl` is a singleton and never destroyed.
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(&WebNNIntrospectionImpl::OnGraphRecordedMainThread,
                            CrossThreadUnretained(this), std::move(json_data)));
    return;
  }

  OnGraphRecordedMainThread(std::move(json_data));
}

void WebNNIntrospectionImpl::OnGraphRecordedMainThread(
    ::mojo_base::BigBuffer json_data) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (client_) {
    client_->OnGraphRecorded(std::move(json_data));
  }
}

#endif

}  // namespace blink
