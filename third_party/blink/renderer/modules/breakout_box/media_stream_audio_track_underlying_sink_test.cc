// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/breakout_box/media_stream_audio_track_underlying_sink.h"

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/modules/mediastream/web_media_stream_audio_sink.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/public/web/web_heap.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_sample_format.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/messaging/message_channel.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"
#include "third_party/blink/renderer/core/streams/writable_stream_transferring_optimizer.h"
#include "third_party/blink/renderer/core/workers/worker_thread_test_helper.h"
#include "third_party/blink/renderer/modules/breakout_box/pushable_media_stream_audio_source.h"
#include "third_party/blink/renderer/modules/mediastream/mock_media_stream_audio_sink.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_audio_track.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component_impl.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_source.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"

using testing::_;
using testing::StrictMock;

namespace WTF {
template <>
struct CrossThreadCopier<
    std::unique_ptr<blink::WritableStreamTransferringOptimizer>> {
  STATIC_ONLY(CrossThreadCopier);
  using Type = std::unique_ptr<blink::WritableStreamTransferringOptimizer>;
  static Type Copy(Type pointer) { return pointer; }
};
}  // namespace WTF

namespace blink {

class MediaStreamAudioTrackUnderlyingSinkTest : public testing::Test {
 public:
  MediaStreamAudioTrackUnderlyingSinkTest() : testing_thread_("TestingThread") {
    testing_thread_.Start();
    auto pushable_audio_source =
        std::make_unique<PushableMediaStreamAudioSource>(
            scheduler::GetSingleThreadTaskRunnerForTesting(),
            testing_thread_.task_runner());
    pushable_audio_source_ = pushable_audio_source.get();
    media_stream_source_ = MakeGarbageCollected<MediaStreamSource>(
        "dummy_source_id", MediaStreamSource::kTypeAudio, "dummy_source_name",
        /*remote=*/false, std::move(pushable_audio_source));
  }

  ~MediaStreamAudioTrackUnderlyingSinkTest() override {
    WebHeap::CollectAllGarbageForTesting();
    testing_thread_.Stop();
  }

  MediaStreamAudioTrackUnderlyingSink* CreateUnderlyingSink(
      ScriptState* script_state) {
    return MakeGarbageCollected<MediaStreamAudioTrackUnderlyingSink>(
        pushable_audio_source_->GetBroker());
  }

  void CreateTrackAndConnectToSource() {
    media_stream_component_ = MakeGarbageCollected<MediaStreamComponentImpl>(
        media_stream_source_->Id(), media_stream_source_,
        std::make_unique<MediaStreamAudioTrack>(true /* is_local_track */));
    pushable_audio_source_->ConnectToInitializedTrack(media_stream_component_);
  }

  ScriptValue CreateAudioData(ScriptState* script_state,
                              AudioData** audio_data_out = nullptr) {
    const scoped_refptr<media::AudioBuffer> media_buffer =
        media::AudioBuffer::CreateEmptyBuffer(
            media::ChannelLayout::CHANNEL_LAYOUT_STEREO,
            /*channel_count=*/2,
            /*sample_rate=*/44100,
            /*frame_count=*/500, base::TimeDelta());
    AudioData* audio_data =
        MakeGarbageCollected<AudioData>(std::move(media_buffer));
    if (audio_data_out)
      *audio_data_out = audio_data;
    return ScriptValue(script_state->GetIsolate(),
                       ToV8Traits<AudioData>::ToV8(script_state, audio_data));
  }

  static ScriptValue CreateInvalidAudioData(ScriptState* script_state,
                                            ExceptionState& exception_state) {
    AudioDataInit* init = AudioDataInit::Create();
    init->setFormat(V8AudioSampleFormat::Enum::kF32);
    init->setSampleRate(31600.0f);
    init->setNumberOfFrames(316u);
    init->setNumberOfChannels(26u);  // This maps to CHANNEL_LAYOUT_UNSUPPORTED
    init->setTimestamp(1u);
    init->setData(
        MakeGarbageCollected<AllowSharedBufferSource>(DOMArrayBuffer::Create(
            init->numberOfChannels() * init->numberOfFrames(), sizeof(float))));

    AudioData* audio_data =
        AudioData::Create(script_state, init, exception_state);
    return ScriptValue(script_state->GetIsolate(),
                       ToV8Traits<AudioData>::ToV8(script_state, audio_data));
  }

 protected:
  test::TaskEnvironment task_environment_;
  base::Thread testing_thread_;
  Persistent<MediaStreamSource> media_stream_source_;
  Persistent<MediaStreamComponent> media_stream_component_;

  raw_ptr<PushableMediaStreamAudioSource> pushable_audio_source_;
};

TEST_F(MediaStreamAudioTrackUnderlyingSinkTest,
       WriteToStreamForwardsToMediaStreamSink) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);

  CreateTrackAndConnectToSource();

  base::RunLoop write_loop;
  StrictMock<MockMediaStreamAudioSink> mock_sink;
  EXPECT_CALL(mock_sink, OnSetFormat(_)).Times(::testing::AnyNumber());
  EXPECT_CALL(mock_sink, OnData(_, _))
      .WillOnce(base::test::RunOnceClosure(write_loop.QuitClosure()));

  WebMediaStreamAudioSink::AddToAudioTrack(
      &mock_sink, WebMediaStreamTrack(media_stream_component_.Get()));

  NonThrowableExceptionState exception_state;
  auto* writer = writable_stream->getWriter(script_state, exception_state);

  AudioData* audio_data = nullptr;
  auto audio_data_chunk = CreateAudioData(script_state, &audio_data);
  EXPECT_NE(audio_data, nullptr);
  EXPECT_NE(audio_data->data(), nullptr);
  ScriptPromiseTester write_tester(
      script_state,
      writer->write(script_state, audio_data_chunk, exception_state));
  // |audio_data| should be invalidated after sending it to the sink.
  write_tester.WaitUntilSettled();
  EXPECT_EQ(audio_data->data(), nullptr);
  write_loop.Run();

  writer->releaseLock(script_state);
  ScriptPromiseTester close_tester(
      script_state, writable_stream->close(script_state, exception_state));
  close_tester.WaitUntilSettled();

  // Writing to the sink after the stream closes should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  underlying_sink->write(script_state, CreateAudioData(script_state), nullptr,
                         dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));

  WebMediaStreamAudioSink::RemoveFromAudioTrack(
      &mock_sink, WebMediaStreamTrack(media_stream_component_.Get()));
}

TEST_F(MediaStreamAudioTrackUnderlyingSinkTest, WriteInvalidDataFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateUnderlyingSink(script_state);
  ScriptValue v8_integer =
      ScriptValue(script_state->GetIsolate(),
                  v8::Integer::New(script_state->GetIsolate(), 0));

  // Writing something that is not an AudioData to the sink should fail.
  {
    DummyExceptionStateForTesting dummy_exception_state;
    sink->write(script_state, v8_integer, nullptr, dummy_exception_state);
    EXPECT_TRUE(dummy_exception_state.HadException());
  }

  // Writing a null value to the sink should fail.
  {
    DummyExceptionStateForTesting dummy_exception_state;
    EXPECT_FALSE(dummy_exception_state.HadException());
    sink->write(script_state, ScriptValue::CreateNull(v8_scope.GetIsolate()),
                nullptr, dummy_exception_state);
    EXPECT_TRUE(dummy_exception_state.HadException());
  }

  // Writing a closed AudioData to the sink should fail.
  {
    DummyExceptionStateForTesting dummy_exception_state;
    AudioData* audio_data = nullptr;
    auto chunk = CreateAudioData(script_state, &audio_data);
    audio_data->close();
    EXPECT_FALSE(dummy_exception_state.HadException());
    sink->write(script_state, chunk, nullptr, dummy_exception_state);
    EXPECT_TRUE(dummy_exception_state.HadException());
  }
}

TEST_F(MediaStreamAudioTrackUnderlyingSinkTest, WriteToAbortedSinkFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);

  NonThrowableExceptionState exception_state;
  ScriptPromiseTester abort_tester(
      script_state, writable_stream->abort(script_state, exception_state));
  abort_tester.WaitUntilSettled();

  // Writing to the sink after the stream closes should fail.
  DummyExceptionStateForTesting dummy_exception_state;
  underlying_sink->write(script_state, CreateAudioData(script_state), nullptr,
                         dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kInvalidStateError));
}

TEST_F(MediaStreamAudioTrackUnderlyingSinkTest, WriteInvalidAudioDataFails) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* sink = CreateUnderlyingSink(script_state);
  CreateTrackAndConnectToSource();

  DummyExceptionStateForTesting dummy_exception_state;
  auto chunk = CreateInvalidAudioData(script_state, dummy_exception_state);
  EXPECT_FALSE(dummy_exception_state.HadException());

  sink->write(script_state, chunk, nullptr, dummy_exception_state);
  EXPECT_TRUE(dummy_exception_state.HadException());
  EXPECT_EQ(dummy_exception_state.Code(),
            static_cast<ExceptionCode>(DOMExceptionCode::kOperationError));
  EXPECT_EQ(dummy_exception_state.Message(), "Invalid audio data");
}

TEST_F(MediaStreamAudioTrackUnderlyingSinkTest, DeserializeWithOptimizer) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto transfer_optimizer = underlying_sink->GetTransferringOptimizer();
  auto* writable_stream = WritableStream::CreateWithCountQueueingStrategy(
      script_state, underlying_sink, 1u);

  // Transfer the stream using a message port on the main thread.
  auto* channel =
      MakeGarbageCollected<MessageChannel>(v8_scope.GetExecutionContext());
  writable_stream->Serialize(script_state, channel->port1(),
                             ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(writable_stream->IsLocked(writable_stream));

  // Deserialize the stream using the transfer optimizer.
  auto* transferred_stream = WritableStream::Deserialize(
      script_state, channel->port2(), std::move(transfer_optimizer),
      ASSERT_NO_EXCEPTION);
  EXPECT_TRUE(transferred_stream);
  EXPECT_TRUE(pushable_audio_source_->GetBroker()
                  ->ShouldDeliverAudioOnAudioTaskRunner());
}

TEST_F(MediaStreamAudioTrackUnderlyingSinkTest, TransferToWorkerWithOptimizer) {
  V8TestingScope v8_scope;
  ScriptState* script_state = v8_scope.GetScriptState();
  auto* underlying_sink = CreateUnderlyingSink(script_state);
  auto transfer_optimizer = underlying_sink->GetTransferringOptimizer();
  EXPECT_TRUE(pushable_audio_source_->GetBroker()
                  ->ShouldDeliverAudioOnAudioTaskRunner());

  // Start a worker.
  WorkerReportingProxy proxy;
  WorkerThreadForTest worker_thread(proxy);
  worker_thread.StartWithSourceCode(v8_scope.GetWindow().GetSecurityOrigin(),
                                    "/* no worker script */");

  // Create a transferred writable stream on the worker. The optimizer has all
  // the state needed to create the transferred stream.
  // Intentionally keep a reference to the worker task runner on this thread
  // while this occurs.
  scoped_refptr<base::SingleThreadTaskRunner> worker_task_runner =
      worker_thread.GetWorkerBackingThread().BackingThread().GetTaskRunner();
  PostCrossThreadTask(
      *worker_task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](WorkerThread* worker_thread,
             std::unique_ptr<WritableStreamTransferringOptimizer>
                 transfer_optimizer) {
            auto* worker_global_scope = worker_thread->GlobalScope();
            auto* script_controller = worker_global_scope->ScriptController();
            EXPECT_TRUE(script_controller->IsContextInitialized());

            ScriptState* worker_script_state =
                script_controller->GetScriptState();
            ScriptState::Scope worker_scope(worker_script_state);

            // Deserialize using the optimizer.
            auto* transferred_stream = WritableStream::Deserialize(
                worker_script_state,
                MakeGarbageCollected<MessageChannel>(worker_global_scope)
                    ->port2(),
                std::move(transfer_optimizer), ASSERT_NO_EXCEPTION);
            EXPECT_TRUE(transferred_stream);
          },
          CrossThreadUnretained(&worker_thread),
          std::move(transfer_optimizer)));

  // Wait for another task on the worker to finish to ensure that the Oilpan
  // references held by the first task are dropped.
  base::WaitableEvent done;
  PostCrossThreadTask(*worker_task_runner, FROM_HERE,
                      CrossThreadBindOnce(&base::WaitableEvent::Signal,
                                          CrossThreadUnretained(&done)));
  done.Wait();
  EXPECT_FALSE(pushable_audio_source_->GetBroker()
                   ->ShouldDeliverAudioOnAudioTaskRunner());

  // Shut down the worker thread.
  worker_thread.Terminate();
  worker_thread.WaitForShutdownForTesting();
}

}  // namespace blink
