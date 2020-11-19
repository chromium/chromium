// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/image_decoder_external.h"

#include "base/feature_list.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_tester.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_frame.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_image_track.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

class ImageDecoderTest : public testing::Test {
 protected:
  ImageDecoderExternal* CreateDecoder(V8TestingScope* v8_scope,
                                      const char* file_name,
                                      const char* mime_type) {
    auto* init = MakeGarbageCollected<ImageDecoderInit>();
    init->setType(mime_type);

    auto data = ReadFile(file_name);
    DCHECK(!data->IsEmpty()) << "Missing file: " << file_name;
    init->setData(ArrayBufferOrArrayBufferViewOrReadableStream::FromArrayBuffer(
        DOMArrayBuffer::Create(std::move(data))));
    return ImageDecoderExternal::Create(v8_scope->GetScriptState(), init,
                                        v8_scope->GetExceptionState());
  }

  ImageFrameExternal* ToImageFrame(V8TestingScope* v8_scope,
                                   ScriptValue value) {
    return NativeValueTraits<ImageFrameExternal>::NativeValue(
        v8_scope->GetIsolate(), value.V8Value(), v8_scope->GetExceptionState());
  }

  scoped_refptr<SharedBuffer> ReadFile(StringView file_name) {
    StringBuilder file_path;
    file_path.Append(test::BlinkWebTestsDir());
    file_path.Append('/');
    file_path.Append(file_name);
    return test::ReadFromFile(file_path.ToString());
  }
};

TEST_F(ImageDecoderTest, CanDecodeType) {
  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/jpeg"));
  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/pjpeg"));
  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/jpg"));

  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/png"));
  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/x-png"));
  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/apng"));

  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/gif"));

  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/webp"));

  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/x-icon"));
  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/vnd.microsoft.icon"));

  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/bmp"));
  EXPECT_TRUE(ImageDecoderExternal::canDecodeType("image/x-xbitmap"));

#if BUILDFLAG(ENABLE_AV1_DECODER)
  EXPECT_EQ(ImageDecoderExternal::canDecodeType("image/avif"),
            base::FeatureList::IsEnabled(features::kAVIF));
#else
  EXPECT_FALSE(ImageDecoderExternal::canDecodeType("image/avif"));
#endif

  EXPECT_FALSE(ImageDecoderExternal::canDecodeType("image/svg+xml"));
  EXPECT_FALSE(ImageDecoderExternal::canDecodeType("image/heif"));
  EXPECT_FALSE(ImageDecoderExternal::canDecodeType("image/pcx"));
  EXPECT_FALSE(ImageDecoderExternal::canDecodeType("image/bpg"));
}

TEST_F(ImageDecoderTest, DecodeEmpty) {
  V8TestingScope v8_scope;

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType("image/png");
  init->setData(ArrayBufferOrArrayBufferViewOrReadableStream::FromArrayBuffer(
      DOMArrayBuffer::Create(SharedBuffer::Create())));
  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  EXPECT_TRUE(decoder);
  EXPECT_TRUE(v8_scope.GetExceptionState().HadException());
}

TEST_F(ImageDecoderTest, DecodeNeuteredAtConstruction) {
  V8TestingScope v8_scope;

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  auto* buffer = DOMArrayBuffer::Create(SharedBuffer::Create());

  init->setType("image/png");
  init->setData(
      ArrayBufferOrArrayBufferViewOrReadableStream::FromArrayBuffer(buffer));

  ArrayBufferContents contents;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), contents));

  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  EXPECT_TRUE(decoder);
  EXPECT_TRUE(v8_scope.GetExceptionState().HadException());
}

TEST_F(ImageDecoderTest, DecodeNeuteredAtDecodeTime) {
  V8TestingScope v8_scope;

  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(ImageDecoderExternal::canDecodeType(kImageType));

  auto* init = MakeGarbageCollected<ImageDecoderInit>();
  init->setType(kImageType);

  constexpr char kTestFile[] = "images/resources/animated.gif";
  auto data = ReadFile(kTestFile);
  DCHECK(!data->IsEmpty()) << "Missing file: " << kTestFile;

  auto* buffer = DOMArrayBuffer::Create(std::move(data));

  init->setData(
      ArrayBufferOrArrayBufferViewOrReadableStream::FromArrayBuffer(buffer));

  auto* decoder = ImageDecoderExternal::Create(v8_scope.GetScriptState(), init,
                                               v8_scope.GetExceptionState());
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  ArrayBufferContents contents;
  ASSERT_TRUE(buffer->Transfer(v8_scope.GetIsolate(), contents));

  auto promise = decoder->decode(0, true);
  ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());
}

TEST_F(ImageDecoderTest, DecodeUnsupported) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/svg+xml";
  EXPECT_FALSE(ImageDecoderExternal::canDecodeType(kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/test.svg", kImageType);
  EXPECT_TRUE(decoder);
  EXPECT_TRUE(v8_scope.GetExceptionState().HadException());
}

TEST_F(ImageDecoderTest, DecodeGif) {
  V8TestingScope v8_scope;
  constexpr char kImageType[] = "image/gif";
  EXPECT_TRUE(ImageDecoderExternal::canDecodeType(kImageType));
  auto* decoder =
      CreateDecoder(&v8_scope, "images/resources/animated.gif", kImageType);
  ASSERT_TRUE(decoder);
  ASSERT_FALSE(v8_scope.GetExceptionState().HadException());

  {
    auto promise = decoder->decodeMetadata();
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
  }

  EXPECT_EQ(decoder->type(), kImageType);
  EXPECT_EQ(decoder->frameCount(), 2u);
  EXPECT_EQ(decoder->repetitionCount(), 0u);
  EXPECT_EQ(decoder->complete(), true);

  auto tracks = decoder->tracks();
  EXPECT_EQ(tracks.size(), 1u);
  EXPECT_EQ(tracks[0]->id(), 0u);
  EXPECT_EQ(tracks[0]->animated(), true);

  {
    auto promise = decoder->decode(0, true);
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* frame = ToImageFrame(&v8_scope, tester.Value());
    EXPECT_TRUE(frame->complete());
    EXPECT_EQ(frame->duration(), 0u);
    EXPECT_EQ(frame->orientation(), 1u);

    auto* bitmap = frame->image();
    EXPECT_EQ(bitmap->Size(), IntSize(16, 16));
  }

  {
    auto promise = decoder->decode(1, true);
    ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
    tester.WaitUntilSettled();
    ASSERT_TRUE(tester.IsFulfilled());
    auto* frame = ToImageFrame(&v8_scope, tester.Value());
    EXPECT_TRUE(frame->complete());
    EXPECT_EQ(frame->duration(), 0u);
    EXPECT_EQ(frame->orientation(), 1u);

    auto* bitmap = frame->image();
    EXPECT_EQ(bitmap->Size(), IntSize(16, 16));
  }

  // Decoding past the end should result in a rejected promise.
  auto promise = decoder->decode(3, true);
  ScriptPromiseTester tester(v8_scope.GetScriptState(), promise);
  tester.WaitUntilSettled();
  ASSERT_TRUE(tester.IsRejected());
}

// TODO(crbug.com/1073995): Add tests for each format, selectTrack(), partial
// decoding, and ImageBitmapOptions.

}  // namespace

}  // namespace blink
