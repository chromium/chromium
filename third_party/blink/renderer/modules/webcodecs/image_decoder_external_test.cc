// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"

#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybufferallowshared_arraybufferviewallowshared_readablestream.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decode_result.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_track.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/test_underlying_source.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/image_track.h"
#include "third_party/blink/renderer/modules/webcodecs/image_track_list.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class ImageDecoderTest : public testing::Test {
 public:
  ~ImageDecoderTest() override {
    // Force GC before exiting since ImageDecoderExternal will create objects
    // on background threads that will race with the next test's startup. See
    // https://crbug.com/1196376
    ThreadState::Current()->CollectAllGarbageForTesting();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  ImageDecoderExternal* CreateDecoder(V8TestingScope* v8_scope,
                                      const char* file_name,
                                      const char* mime_type) {
    auto* init = MakeGarbageCollected<ImageDecoderInit>();
    init->setType(mime_type);

    init->setData(MakeGarbageCollected<V8ImageBufferSource>(
        DOMArrayBuffer::Create(base::as_byte_span(ReadFile(file_name)))));
    return ImageDecoderExternal::Create(v8_scope->GetScriptState(), init,
                                        v8_scope->GetExceptionState());
  }

  ImageDecodeResult* ToImageDecodeResult(V8TestingScope* v8_scope,
                                         ScriptValue value) {
    return NativeValueTraits<ImageDecodeResult>::NativeValue(
        v8_scope->GetIsolate(), value.V8Value(), v8_scope->GetExceptionState());
  }

  ImageDecodeOptions* MakeOptions(uint32_t frame_index = 0,
                                  bool complete_frames_only = true) {
    auto* options = MakeGarbageCollected<ImageDecodeOptions>();
    options->setFrameIndex(frame_index);
    options->setCompleteFramesOnly(complete_frames_only);
    return options;
  }

  Vector<char> ReadFile(StringView file_name) {
    StringBuilder file_path;
    file_path.Append(test::BlinkWebTestsDir());
    file_path.Append('/');
    file_path.Append(file_name);
    std::optional<Vector<char>> data = test::ReadFromFile(file_path.ToString());
    CHECK(data && data->size()) << "Missing file: " << file_name;
    return std::move(*data);
  }

  bool IsTypeSupported(V8TestingScope* v8_scope, String type) {
    auto promise =
        ImageDecoderExternal::isTypeSupported(v8_scope->GetScriptState(), type);
    ScriptPromiseTester tester(v8_scope->GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_FALSE(tester.IsRejected());

    auto v8_value = tester.Value().V8Value();
    EXPECT_TRUE(v8_value->IsBoolean());
    return v8_value.As<v8::Boolean>()->Value();
  }

  static bool HasAv1Decoder() {
#if BUILDFLAG(ENABLE_AV1_DECODER)
    return true;
#else
    return false;
#endif
  }
  test::TaskEnvironment task_environment_;
};

TEST_F(ImageDecoderTest, IsTypeSupported) {
  V8TestingScope v8_scope;
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/jpeg"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/pjpeg"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/jpg"));

  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/png"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/x-png"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/apng"));

  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/gif"));

  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/webp"));

  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/bmp"));
  EXPECT_TRUE(IsTypeSupported(&v8_scope, "image/x-xbitmap"));

  EXPECT_EQ(IsTypeSupported(&v8_scope, "image/avif"), HasAv1Decoder());

  EXPECT_FALSE(IsTypeSupported(&v8_scope, "image/x-icon"));
  EXPECT_FALSE(IsTypeSupported(&v8_scope, "image/vnd.microsoft.icon"));
  EXPECT_FALSE(IsTypeSupported(&v8_scope, "image/svg+xml"));
  EXPECT_FALSE(IsTypeSupported(&v8_scope, "image/heif"));
  EXPECT_FALSE(IsTypeSupported(&v8_scope, "image/pcx"));
  EXPECT_FALSE(IsTypeSupported(&v8_scope, "image/bpg"));
}

TEST_F(ImageDecoderTest, DecodeEmpty) {
  V8TestingScope v8_scope;

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType("image/png");
  init->setData(MakeGarbageCollected<V8ImageBufferSource>(
      DOMArrayBuffer::Create(SharedBuffer::Create())));
  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  EXPECT_FALSE(decoder);
  EXPECT_TRUE(v8_scope.GetExceptionState().HadException());
}

TEST_F(ImageDecoderTest, DecodeNeuteredAtConstruction) {
  V8TestingScope v8_scope;

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  auto* buffer = DOMArrayBuffer::Create(SharedBuffer::Create());

  init->setType("image/png");
  init->setData(MakeGarbageCollected<V8ImageBufferSource>(buffer));

  ArrayBufferContents contents;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), contents,
                               v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  EXPECT_FALSE(decoder);
  EXPECT_TRUE(v8_scope.GetExceptionState().HadException());
}

TEST_F(ImageDecoderTest, DecodeNeuteredAtDecodeTime) {
  V8TestingScope v8_scope;

  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);

  constexpr char kTestFile[] = "images/resources/animated.gif";
  Vector<char> data = ReadFile(kTestFile);

  auto* buffer = DOMArrayBuffer::Create(base::as_byte_span(data));

  init->setData(MakeGarbageCollected<V8ImageBufferSource>(buffer));

  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  ArrayBufferContents contents;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), contents,
                               v8_scope.GetExceptionState()));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  auto promise = decoder->decode(MakeOptions(0, true));
  ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_FALSE(tester.IsRejected());
}

TEST_F(ImageDecoderTest, DecodeUnsupported) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/svg+xml";
  EXPECT_FALSE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/test.svg", kImageType);
  EXPECT_TRUE(decoder);
  EXPECT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }
}

TEST_F(ImageDecoderTest, DecoderCreationMixedCaseMimeType) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/GiF";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), "image/gif");
}

TEST_F(ImageDecoderTest, DecodeGifZeroDuration) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->timestamp(), 0u);
    EXPECT_EQ(frame->duration(), 0u);
    EXPECT_EQ(frame->displayWidth(), 16u);
    EXPECT_EQ(frame->displayHeight(), 16u);
    EXPECT_EQ(frame->frame()->ColorSpace(), gfx::ColorSpace::CreateSRGB());
  }

  {
    auto promise = decoder->decode(MakeOptions(1, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->timestamp(), 0u);
    EXPECT_EQ(frame->duration(), 0u);
    EXPECT_EQ(frame->displayWidth(), 16u);
    EXPECT_EQ(frame->displayHeight(), 16u);
    EXPECT_EQ(frame->frame()->ColorSpace(), gfx::ColorSpace::CreateSRGB());
  }

  // Decoding past the end should result in a rejected promise.
  auto promise = decoder->decode(MakeOptions(3, true));
  ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());
}

TEST_F(ImageDecoderTest, DecodeGif) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder = CreateDecoder(
      &v8_scope, "images/resources/animated-10color.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  const auto& tracks = decoder->tracks();
  ASSERT_EQ(tracks.length(), 1u);
  EXPECT_EQ(tracks.AnonymousIndexedGetter(0)->animated(), true);
  EXPECT_EQ(tracks.selectedTrack()->animated(), true);

  EXPECT_EQ(decoder->type(), kImageType);
  EXPECT_EQ(tracks.selectedTrack()->frameCount(), 10u);
  EXPECT_EQ(tracks.selectedTrack()->repetitionCount(), INFINITY);
  EXPECT_EQ(decoder->complete(), true);

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->timestamp(), 0u);
    EXPECT_EQ(frame->duration(), 100000u);
    EXPECT_EQ(frame->displayWidth(), 100u);
    EXPECT_EQ(frame->displayHeight(), 100u);
    EXPECT_EQ(frame->frame()->ColorSpace(), gfx::ColorSpace::CreateSRGB());
  }

  {
    auto promise = decoder->decode(MakeOptions(1, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->timestamp(), 100000u);
    EXPECT_EQ(frame->duration(), 100000u);
    EXPECT_EQ(frame->displayWidth(), 100u);
    EXPECT_EQ(frame->displayHeight(), 100u);
    EXPECT_EQ(frame->frame()->ColorSpace(), gfx::ColorSpace::CreateSRGB());
  }

  // Decoding past the end should result in a rejected promise.
  auto promise = decoder->decode(MakeOptions(11, true));
  ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());
}

TEST_F(ImageDecoderTest, DecodeCompleted) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->completed(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }
}

TEST_F(ImageDecoderTest, DecodeAborted) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/avif";
  EXPECT_EQ(IsTypeSupported(&v8_scope, kImageType), HasAv1Decoder());

  // Use an expensive-to-decode image to try and ensure work exists to abort.
  auto* decoder = CreateDecoder(
      &v8_scope,
      "images/resources/avif/red-at-12-oclock-with-color-profile-12bpc.avif",
      kImageType);

  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_EQ(tester.IsFulfilled(), HasAv1Decoder());
  }

  // Setup a scenario where there should be work to abort. Since blink tests use
  // real threads with the base::TaskEnvironment, we can't actually be sure that
  // work hasn't completed by the time reset() is called.
  for (int i = 0; i < 10; ++i)
    decoder->decode();
  decoder->reset();

  // There's no way to verify work was aborted, so just ensure nothing explodes.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ImageDecoderTest, DecoderReset) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), "image/gif");
  decoder->reset();

  // Ensure decoding works properly after reset.
  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  const auto& tracks = decoder->tracks();
  ASSERT_EQ(tracks.length(), 1u);
  EXPECT_EQ(tracks.AnonymousIndexedGetter(0)->animated(), true);
  EXPECT_EQ(tracks.selectedTrack()->animated(), true);

  EXPECT_EQ(decoder->type(), kImageType);
  EXPECT_EQ(tracks.selectedTrack()->frameCount(), 2u);
  EXPECT_EQ(tracks.selectedTrack()->repetitionCount(), INFINITY);
  EXPECT_EQ(decoder->complete(), true);

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->duration(), 0u);
    EXPECT_EQ(frame->displayWidth(), 16u);
    EXPECT_EQ(frame->displayHeight(), 16u);
  }
}

TEST_F(ImageDecoderTest, DecoderClose) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), "image/gif");
  decoder->close();

  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }
}

TEST_F(ImageDecoderTest, DecoderContextDestroyed) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), "image/gif");

  // Decoder creation will queue metadata decoding which should be counted as
  // pending activity.
  EXPECT_TRUE(decoder->HasPendingActivity());
  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
  }

  // After metadata resolution completes, we should return to no activity.
  EXPECT_FALSE(decoder->HasPendingActivity());

  // Queue some activity.
  decoder->decode();
  EXPECT_TRUE(decoder->HasPendingActivity());

  // Destroying the context should close() the decoder and stop all activity.
  v8_scope.GetExecutionContext()->NotifyContextDestroyed();
  EXPECT_FALSE(decoder->HasPendingActivity());

  // Promises won't resolve or reject now that the context is destroyed, but we
  // should ensure decode() doesn't trigger any issues.
  decoder->decode(MakeOptions(0, true));

  // This will fail if a decode() or metadata decode was queued.
  EXPECT_FALSE(decoder->HasPendingActivity());
}

TEST_F(ImageDecoderTest, DecoderContextDestroyedBeforeCreation) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  // Destroying the context prior to construction should fail creation.
  v8_scope.GetExecutionContext()->NotifyContextDestroyed();

  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_FALSE(decoder);
  ASSERT_TRUE(v8_scope.GetExceptionState().HadException());
}

TEST_F(ImageDecoderTest, DecoderReadableStream) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  Vector<char> data = ReadFile("images/resources/animated-10color.gif");

  Persistent<TestUnderlyingSource> underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(v8_scope.GetScriptState());
  Persistent<ReadableStream> stream =
      ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                      underlying_source, 0);

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);
  init->setData(MakeGarbageCollected<V8ImageBufferSource>(stream));

  Persistent<ImageDecoderExternal> decoder = ImageDecoderExternal::Create(
      v8_scope.GetScriptState(), init, IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), kImageType);

  constexpr size_t kNumChunks = 2;
  const size_t chunk_size = (data.size() + 1) / kNumChunks;
  base::span<const uint8_t> data_span = base::as_byte_span(data);

  v8::Local<v8::Value> v8_data_array = ToV8Traits<DOMUint8Array>::ToV8(
      v8_scope.GetScriptState(),
      DOMUint8Array::Create(data_span.subspan(0, chunk_size)));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  underlying_source->Enqueue(ScriptValue(v8_scope.GetIsolate(), v8_data_array));

  // Ensure we have metadata.
  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
  }

  // Deselect the current track.
  ASSERT_TRUE(decoder->tracks().selectedTrack());
  decoder->tracks().selectedTrack()->setSelected(false);

  // Enqueue remaining data.
  v8_data_array = ToV8Traits<DOMUint8Array>::ToV8(
      v8_scope.GetScriptState(), DOMUint8Array::Create(data_span.subspan(
                                     chunk_size, data.size() - chunk_size)));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  underlying_source->Enqueue(ScriptValue(v8_scope.GetIsolate(), v8_data_array));
  underlying_source->Close();

  // Completed will not resolve while we have no selected track.
  auto completed_promise = decoder->completed(v8_scope.GetScriptState());
  ScriptPromiseTester completed_tester(v8_scope.GetScriptState(),
                                       completed_promise);
  EXPECT_FALSE(completed_tester.IsFulfilled());
  EXPECT_FALSE(completed_tester.IsRejected());

  // Metadata should resolve okay while no track is selected.
  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
  }

  // Decodes should be rejected while no track is selected.
  {
    auto promise = decoder->decode();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }

  EXPECT_FALSE(completed_tester.IsFulfilled());
  EXPECT_FALSE(completed_tester.IsRejected());

  // Select a track again.
  decoder->tracks().AnonymousIndexedGetter(0)->setSelected(true);

  completed_tester.WaitUntilSettled();
  EXPECT_TRUE(completed_tester.IsFulfilled());

  // Verify a decode completes successfully.
  {
    auto promise = decoder->decode();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->timestamp(), 0u);
    EXPECT_EQ(*frame->duration(), 100000u);
    EXPECT_EQ(frame->displayWidth(), 100u);
    EXPECT_EQ(frame->displayHeight(), 100u);
    EXPECT_EQ(frame->frame()->ColorSpace(), gfx::ColorSpace::CreateSRGB());
  }
}

TEST_F(ImageDecoderTest, DecoderReadableStreamAvif) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/avif";
  EXPECT_EQ(IsTypeSupported(&v8_scope, kImageType), HasAv1Decoder());

  Vector<char> data = ReadFile("images/resources/avif/star-animated-8bpc.avif");

  Persistent<TestUnderlyingSource> underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(v8_scope.GetScriptState());
  Persistent<ReadableStream> stream =
      ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                      underlying_source, 0);

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);
  init->setData(MakeGarbageCollected<V8ImageBufferSource>(stream));

  Persistent<ImageDecoderExternal> decoder = ImageDecoderExternal::Create(
      v8_scope.GetScriptState(), init, IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), kImageType);

  // Enqueue a single byte and ensure nothing breaks.
  base::span<const uint8_t> data_span = base::as_byte_span(data);
  v8::Local<v8::Value> v8_data_array = ToV8Traits<DOMUint8Array>::ToV8(
      v8_scope.GetScriptState(),
      DOMUint8Array::Create(data_span.subspan(0, 1)));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  underlying_source->Enqueue(ScriptValue(v8_scope.GetIsolate(), v8_data_array));

  auto metadata_promise = decoder->tracks().ready(v8_scope.GetScriptState());
  auto decode_promise = decoder->decode();
  base::RunLoop().RunUntilIdle();

  // One byte shouldn't be enough to decode size or fail, so no promises should
  // be resolved.
  ScriptPromiseTester metadata_tester(v8_scope.GetScriptState(),
                                      metadata_promise);
  EXPECT_FALSE(metadata_tester.IsFulfilled());
  EXPECT_FALSE(metadata_tester.IsRejected());

  ScriptPromiseTester decode_tester(v8_scope.GetScriptState(), decode_promise);
  EXPECT_FALSE(decode_tester.IsFulfilled());
  EXPECT_FALSE(decode_tester.IsRejected());

  // Append the rest of the data.
  v8_data_array = ToV8Traits<DOMUint8Array>::ToV8(
      v8_scope.GetScriptState(),
      DOMUint8Array::Create(data_span.subspan(1, data.size() - 1)));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  underlying_source->Enqueue(ScriptValue(v8_scope.GetIsolate(), v8_data_array));

  // Ensure we have metadata.
  metadata_tester.WaitUntilSettled();
  ASSERT_EQ(metadata_tester.IsFulfilled(), HasAv1Decoder());

  // Verify decode completes successfully.
  decode_tester.WaitUntilSettled();
#if BUILDFLAG(ENABLE_AV1_DECODER)
  ASSERT_TRUE(decode_tester.IsFulfilled());
  auto* result = ToImageDecodeResult(&v8_scope, decode_tester.Value());
  EXPECT_TRUE(result->complete());

  auto* frame = result->image();
  EXPECT_EQ(frame->timestamp(), 0u);
  EXPECT_EQ(*frame->duration(), 100000u);
  EXPECT_EQ(frame->displayWidth(), 159u);
  EXPECT_EQ(frame->displayHeight(), 159u);
  EXPECT_EQ(frame->frame()->ColorSpace(), gfx::ColorSpace::CreateSRGB());
#else
  EXPECT_FALSE(decode_tester.IsFulfilled());
#endif
}

TEST_F(ImageDecoderTest, ReadableStreamAvifStillYuvDecoding) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/avif";
  EXPECT_EQ(IsTypeSupported(&v8_scope, kImageType), HasAv1Decoder());

  Vector<char> data =
      ReadFile("images/resources/avif/red-limited-range-420-8bpc.avif");

  Persistent<TestUnderlyingSource> underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(v8_scope.GetScriptState());
  Persistent<ReadableStream> stream =
      ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                      underlying_source, 0);

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);
  init->setData(MakeGarbageCollected<V8ImageBufferSource>(stream));

  Persistent<ImageDecoderExternal> decoder = ImageDecoderExternal::Create(
      v8_scope.GetScriptState(), init, IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), kImageType);

  // Append all data, but don't mark the stream as complete yet.
  v8::Local<v8::Value> v8_data_array = ToV8Traits<DOMUint8Array>::ToV8(
      v8_scope.GetScriptState(),
      DOMUint8Array::Create(base::as_byte_span(data)));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  underlying_source->Enqueue(ScriptValue(v8_scope.GetIsolate(), v8_data_array));

  // Wait for metadata so we know the append has occurred.
  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_EQ(tester.IsFulfilled(), HasAv1Decoder());
  }

  // Attempt to decode a frame greater than the first.
  auto bad_promise = decoder->decode(MakeOptions(1, true));
  base::RunLoop().RunUntilIdle();

  // Mark the stream as complete.
  underlying_source->Close();

  // Now that all data is in we see only 1 frame and request should be rejected.
  {
    ScriptPromiseTester tester(v8_scope.GetScriptState(), bad_promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }

  {
    auto promise = decoder->decode();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
#if BUILDFLAG(ENABLE_AV1_DECODER)
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->format(), "I420");
    EXPECT_EQ(frame->timestamp(), 0u);
    EXPECT_EQ(frame->duration(), std::nullopt);
    EXPECT_EQ(frame->displayWidth(), 3u);
    EXPECT_EQ(frame->displayHeight(), 3u);
    EXPECT_EQ(frame->frame()->ColorSpace(),
              gfx::ColorSpace(gfx::ColorSpace::PrimaryID::BT709,
                              gfx::ColorSpace::TransferID::SRGB,
                              gfx::ColorSpace::MatrixID::BT709,
                              gfx::ColorSpace::RangeID::LIMITED));
#else
    EXPECT_FALSE(tester.IsFulfilled());
#endif
  }
}

TEST_F(ImageDecoderTest, DecodePartialImage) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/png";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);

  // Read just enough to get the header and some of the image data.
  Vector<char> data = ReadFile("images/resources/dice.png");
  auto* array_buffer = DOMArrayBuffer::Create(128, 1);
  array_buffer->ByteSpan().copy_from(
      base::as_byte_span(data).subspan(0, array_buffer->ByteLength()));

  init->setData(MakeGarbageCollected<V8ImageBufferSource>(array_buffer));
  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  {
    auto promise1 = decoder->decode();
    auto promise2 = decoder->decode(MakeOptions(2, true));

    ScriptPromiseTester tester1(v8_scope.GetScriptState(), promise1);
    ScriptPromiseTester tester2(v8_scope.GetScriptState(), promise2);

    // Order is inverted here to catch a specific issue where out of range
    // resolution is handled ahead of decode. https://crbug.com/1200137.
    tester2.WaitUntilSettled();
    ASSERT_TRUE(tester2.IsRejected());

    tester1.WaitUntilSettled();
    ASSERT_TRUE(tester1.IsRejected());
  }
}

TEST_F(ImageDecoderTest, DecodeClosedDuringReadableStream) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  Vector<char> data = ReadFile("images/resources/animated-10color.gif");

  Persistent<TestUnderlyingSource> underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(v8_scope.GetScriptState());
  Persistent<ReadableStream> stream =
      ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                      underlying_source, 0);

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);
  init->setData(MakeGarbageCollected<V8ImageBufferSource>(stream));

  Persistent<ImageDecoderExternal> decoder = ImageDecoderExternal::Create(
      v8_scope.GetScriptState(), init, IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), kImageType);

  base::span<const uint8_t> data_span = base::as_byte_span(data);

  v8::Local<v8::Value> v8_data_array = ToV8Traits<DOMUint8Array>::ToV8(
      v8_scope.GetScriptState(),
      DOMUint8Array::Create(data_span.subspan(0, data.size() / 2)));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  underlying_source->Enqueue(ScriptValue(v8_scope.GetIsolate(), v8_data_array));

  // Ensure we have metadata.
  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
  }

  auto promise = decoder->completed(v8_scope.GetScriptState());
  ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tester.IsFulfilled());
  EXPECT_FALSE(tester.IsRejected());
  decoder->close();
  tester.WaitUntilSettled();
  EXPECT_TRUE(tester.IsRejected());
}

TEST_F(ImageDecoderTest, DecodeInvalidFileViaReadableStream) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/webp";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  Vector<char> data = ReadFile("images/resources/invalid-animated-webp.webp");

  Persistent<TestUnderlyingSource> underlying_source =
      MakeGarbageCollected<TestUnderlyingSource>(v8_scope.GetScriptState());
  Persistent<ReadableStream> stream =
      ReadableStream::CreateWithCountQueueingStrategy(v8_scope.GetScriptState(),
                                                      underlying_source, 0);

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);
  init->setData(MakeGarbageCollected<V8ImageBufferSource>(stream));

  Persistent<ImageDecoderExternal> decoder = ImageDecoderExternal::Create(
      v8_scope.GetScriptState(), init, IGNORE_EXCEPTION_FOR_TESTING);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());
  EXPECT_EQ(decoder->type(), kImageType);

  base::span<const uint8_t> data_span = base::as_byte_span(data);

  v8::Local<v8::Value> v8_data_array = ToV8Traits<DOMUint8Array>::ToV8(
      v8_scope.GetScriptState(),
      DOMUint8Array::Create(data_span.subspan(0, data.size() / 2)));
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  underlying_source->Enqueue(ScriptValue(v8_scope.GetIsolate(), v8_data_array));

  // Ensure we have metadata.
  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsFulfilled());
  }

  auto completed_promise = decoder->completed(v8_scope.GetScriptState());
  ScriptPromiseTester completed_tester(v8_scope.GetScriptState(),
                                       completed_promise);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(completed_tester.IsFulfilled());
  EXPECT_FALSE(completed_tester.IsRejected());

  {
    auto promise = decoder->decode(
        MakeOptions(decoder->tracks().selectedTrack()->frameCount() - 1, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    EXPECT_TRUE(tester.IsRejected());
  }

  completed_tester.WaitUntilSettled();
  EXPECT_TRUE(completed_tester.IsRejected());
}

TEST_F(ImageDecoderTest, DecodeYuv) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/jpeg";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/ycbcr-420.jpg", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->tracks().ready(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  const auto& tracks = decoder->tracks();
  ASSERT_EQ(tracks.length(), 1u);
  EXPECT_EQ(tracks.AnonymousIndexedGetter(0)->animated(), false);
  EXPECT_EQ(tracks.selectedTrack()->animated(), false);

  EXPECT_EQ(decoder->type(), kImageType);
  EXPECT_EQ(tracks.selectedTrack()->frameCount(), 1u);
  EXPECT_EQ(tracks.selectedTrack()->repetitionCount(), 0);
  EXPECT_EQ(decoder->complete(), true);

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->format(), "I420");
    EXPECT_EQ(frame->timestamp(), 0u);
    EXPECT_EQ(frame->duration(), std::nullopt);
    EXPECT_EQ(frame->displayWidth(), 99u);
    EXPECT_EQ(frame->displayHeight(), 99u);
    EXPECT_EQ(frame->frame()->ColorSpace(), gfx::ColorSpace::CreateJpeg());
  }
}

TEST_F(ImageDecoderTest, TransferBuffer) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(IsTypeSupported(&v8_scope, kImageType));

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);

  Vector<char> data = ReadFile("images/resources/animated.gif");

  auto* buffer = DOMArrayBuffer::Create(base::as_byte_span(data));
  init->setData(MakeGarbageCollected<V8ImageBufferSource>(buffer));

  HeapVector<Member<DOMArrayBuffer>> transfer;
  transfer.push_back(Member<DOMArrayBuffer>(buffer));
  init->setTransfer(std::move(transfer));

  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  ASSERT_TRUE(decoder);
  EXPECT_TRUE(buffer->IsDetached());
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->completed(v8_scope.GetScriptState());
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  {
    auto promise = decoder->decode(MakeOptions(0, true));
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* result = ToImageDecodeResult(&v8_scope, tester.Value());
    EXPECT_TRUE(result->complete());

    auto* frame = result->image();
    EXPECT_EQ(frame->duration(), 0u);
    EXPECT_EQ(frame->displayWidth(), 16u);
    EXPECT_EQ(frame->displayHeight(), 16u);
  }
}

}  // namespace

}  // namespace blink
