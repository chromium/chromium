// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/script_decoder.h"

#include <memory>

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/platform/loader/fetch/response_body_loader_client.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_std.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"

namespace WTF {

template <>
struct CrossThreadCopier<blink::ScriptDecoder::Result> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = blink::ScriptDecoder::Result;
  static Type Copy(Type&& value) { return std::move(value); }
};

template <>
struct CrossThreadCopier<mojo::ScopedDataPipeConsumerHandle> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = mojo::ScopedDataPipeConsumerHandle;
  static Type Copy(Type&& value) { return std::move(value); }
};

}  // namespace WTF

namespace blink {

ScriptDecoder::Result::Result(
    SegmentedBuffer raw_data,
    String decoded_data,
    std::unique_ptr<ParkableStringImpl::SecureDigest> digest)
    : raw_data(std::move(raw_data)),
      decoded_data(std::move(decoded_data)),
      digest(std::move(digest)) {}

////////////////////////////////////////////////////////////////////////
// ScriptDecoder
////////////////////////////////////////////////////////////////////////

ScriptDecoder::ScriptDecoder(
    std::unique_ptr<TextResourceDecoder> decoder,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner)
    : decoder_(std::move(decoder)),
      client_task_runner_(std::move(client_task_runner)),
      decoding_task_runner_(worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING})) {}

void ScriptDecoder::DidReceiveData(Vector<char> data) {
  if (!decoding_task_runner_->RunsTasksInCurrentSequence()) {
    PostCrossThreadTask(
        *decoding_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&ScriptDecoder::DidReceiveData,
                            CrossThreadUnretained(this), std::move(data)));
    return;
  }

  CHECK(decoding_task_runner_->RunsTasksInCurrentSequence());
  CHECK(!client_task_runner_->RunsTasksInCurrentSequence());

  AppendData(decoder_->Decode(data));
  raw_data_.Append(std::move(data));
}

void ScriptDecoder::FinishDecode(
    OnDecodeFinishedCallback on_decode_finished_callback) {
  if (!decoding_task_runner_->RunsTasksInCurrentSequence()) {
    PostCrossThreadTask(
        *decoding_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&ScriptDecoder::FinishDecode,
                            CrossThreadUnretained(this),
                            std::move(on_decode_finished_callback)));
    return;
  }

  CHECK(decoding_task_runner_->RunsTasksInCurrentSequence());
  CHECK(!client_task_runner_->RunsTasksInCurrentSequence());

  AppendData(decoder_->Flush());

  DigestValue digest_value;
  digestor_.Finish(digest_value);

  PostCrossThreadTask(
      *client_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          std::move(on_decode_finished_callback),
          Result(std::move(raw_data_), builder_.ReleaseString(),
                 std::make_unique<ParkableStringImpl::SecureDigest>(
                     digest_value))));
}

void ScriptDecoder::Delete() const {
  decoding_task_runner_->DeleteSoon(FROM_HERE, this);
}

void ScriptDecoder::AppendData(const String& data) {
  digestor_.Update(data.RawByteSpan());
  builder_.Append(data);
}

void ScriptDecoderDeleter::operator()(const ScriptDecoder* ptr) {
  if (ptr) {
    ptr->Delete();
  }
}

ScriptDecoderPtr ScriptDecoder::Create(
    std::unique_ptr<TextResourceDecoder> decoder,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner) {
  return ScriptDecoderPtr(
      new ScriptDecoder(std::move(decoder), std::move(client_task_runner)));
}

////////////////////////////////////////////////////////////////////////
// DataPipeScriptDecoder
////////////////////////////////////////////////////////////////////////

DataPipeScriptDecoder::DataPipeScriptDecoder(
    std::unique_ptr<TextResourceDecoder> decoder,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    OnDecodeFinishedCallback on_decode_finished_callback)
    : decoder_(std::move(decoder)),
      client_task_runner_(std::move(client_task_runner)),
      on_decode_finished_callback_(std::move(on_decode_finished_callback)),
      decoding_task_runner_(worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING})) {
  CHECK(features::kBackgroundCodeCacheDecoderStart.Get());
}

void DataPipeScriptDecoder::Start(mojo::ScopedDataPipeConsumerHandle source) {
  if (!decoding_task_runner_->RunsTasksInCurrentSequence()) {
    PostCrossThreadTask(
        *decoding_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&DataPipeScriptDecoder::Start,
                            CrossThreadUnretained(this), std::move(source)));
    return;
  }
  drainer_ = std::make_unique<mojo::DataPipeDrainer>(this, std::move(source));
}

void DataPipeScriptDecoder::OnDataAvailable(base::span<const uint8_t> data) {
  AppendData(decoder_->Decode(data));
  raw_data_.Append(data);
}

void DataPipeScriptDecoder::OnDataComplete() {
  AppendData(decoder_->Flush());
  digestor_.Finish(digest_value_);
  PostCrossThreadTask(
      *client_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          std::move(on_decode_finished_callback_),
          ScriptDecoder::Result(
              std::move(raw_data_), builder_.ReleaseString(),
              std::make_unique<ParkableStringImpl::SecureDigest>(
                  digest_value_))));
}

void DataPipeScriptDecoder::AppendData(const String& data) {
  digestor_.Update(data.RawByteSpan());
  builder_.Append(data);
}

void DataPipeScriptDecoder::Delete() const {
  decoding_task_runner_->DeleteSoon(FROM_HERE, this);
}

void DataPipeScriptDecoderDeleter::operator()(
    const DataPipeScriptDecoder* ptr) {
  if (ptr) {
    ptr->Delete();
  }
}

DataPipeScriptDecoderPtr DataPipeScriptDecoder::Create(
    std::unique_ptr<TextResourceDecoder> decoder,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    OnDecodeFinishedCallback on_decode_finished_callback) {
  return DataPipeScriptDecoderPtr(new DataPipeScriptDecoder(
      std::move(decoder), std::move(client_task_runner),
      std::move(on_decode_finished_callback)));
}

////////////////////////////////////////////////////////////////////////
// ScriptDecoderWithClient
////////////////////////////////////////////////////////////////////////

ScriptDecoderWithClient::ScriptDecoderWithClient(
    ResponseBodyLoaderClient* response_body_loader_client,
    std::unique_ptr<TextResourceDecoder> decoder,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner)
    : decoder_(std::move(decoder)),
      client_task_runner_(std::move(client_task_runner)),
      decoding_task_runner_(worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_BLOCKING})),
      response_body_loader_client_(
          MakeCrossThreadWeakHandle(response_body_loader_client)) {}

void ScriptDecoderWithClient::DidReceiveData(Vector<char> data,
                                             bool send_to_client) {
  if (!decoding_task_runner_->RunsTasksInCurrentSequence()) {
    PostCrossThreadTask(
        *decoding_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&ScriptDecoderWithClient::DidReceiveData,
                            CrossThreadUnretained(this), std::move(data),
                            send_to_client));
    return;
  }

  CHECK(decoding_task_runner_->RunsTasksInCurrentSequence());
  CHECK(!client_task_runner_->RunsTasksInCurrentSequence());

  AppendData(decoder_->Decode(data));

  if (!send_to_client) {
    return;
  }
  PostCrossThreadTask(
      *client_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &ResponseBodyLoaderClient::DidReceiveData,
          MakeUnwrappingCrossThreadWeakHandle(response_body_loader_client_),
          std::move(data)));
}

void ScriptDecoderWithClient::FinishDecode(
    CrossThreadOnceClosure main_thread_continuation) {
  if (!decoding_task_runner_->RunsTasksInCurrentSequence()) {
    PostCrossThreadTask(
        *decoding_task_runner_, FROM_HERE,
        CrossThreadBindOnce(&ScriptDecoderWithClient::FinishDecode,
                            CrossThreadUnretained(this),
                            std::move(main_thread_continuation)));
    return;
  }

  CHECK(decoding_task_runner_->RunsTasksInCurrentSequence());
  CHECK(!client_task_runner_->RunsTasksInCurrentSequence());

  AppendData(decoder_->Flush());

  DigestValue digest_value;
  digestor_.Finish(digest_value);

  PostCrossThreadTask(
      *client_task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          [](ResponseBodyLoaderClient* response_body_loader_client,
             const String& decoded_data,
             std::unique_ptr<ParkableStringImpl::SecureDigest> digest,
             CrossThreadOnceClosure main_thread_continuation) {
            if (response_body_loader_client) {
              response_body_loader_client->DidReceiveDecodedData(
                  decoded_data, std::move(digest));
            }
            std::move(main_thread_continuation).Run();
          },
          MakeUnwrappingCrossThreadWeakHandle(response_body_loader_client_),
          builder_.ReleaseString(),
          std::make_unique<ParkableStringImpl::SecureDigest>(digest_value),
          std::move(main_thread_continuation)));
}

void ScriptDecoderWithClient::Delete() const {
  decoding_task_runner_->DeleteSoon(FROM_HERE, this);
}

void ScriptDecoderWithClient::AppendData(const String& data) {
  digestor_.Update(data.RawByteSpan());
  builder_.Append(data);
}

void ScriptDecoderWithClientDeleter::operator()(
    const ScriptDecoderWithClient* ptr) {
  if (ptr) {
    ptr->Delete();
  }
}

ScriptDecoderWithClientPtr ScriptDecoderWithClient::Create(
    ResponseBodyLoaderClient* response_body_loader_client,
    std::unique_ptr<TextResourceDecoder> decoder,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner) {
  return ScriptDecoderWithClientPtr(new ScriptDecoderWithClient(
      response_body_loader_client, std::move(decoder),
      std::move(client_task_runner)));
}

}  // namespace blink
