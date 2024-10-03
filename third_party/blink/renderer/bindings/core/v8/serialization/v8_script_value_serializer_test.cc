// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_serializer.h"

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "gin/wrappable.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_blob_info.h"
#include "third_party/blink/renderer/bindings/core/v8/script_evaluation_result.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/trailer_reader.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/unpacked_serialized_script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/v8_script_value_deserializer.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_blob.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_matrix_read_only.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_point_read_only.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_quad.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_rect_read_only.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_fenced_frame_config.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_file_list.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_bitmap.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_image_data.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_message_port.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_mojo_handle.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_offscreen_canvas.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_string_resource.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_float32array_uint16array_uint8clampedarray.h"
#include "third_party/blink/renderer/core/context_features/context_feature_settings.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/fileapi/blob.h"
#include "third_party/blink/renderer/core/fileapi/file.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix.h"
#include "third_party/blink/renderer/core/geometry/dom_matrix_read_only.h"
#include "third_party/blink/renderer/core/geometry/dom_point.h"
#include "third_party/blink/renderer/core/geometry/dom_point_read_only.h"
#include "third_party/blink/renderer/core/geometry/dom_quad.h"
#include "third_party/blink/renderer/core/geometry/dom_rect.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/canvas/image_data.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_config.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/core/messaging/message_port.h"
#include "third_party/blink/renderer/core/mojo/mojo_handle.h"
#include "third_party/blink/renderer/core/offscreencanvas/offscreen_canvas.h"
#include "third_party/blink/renderer/core/script/classic_script.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/transform_stream.h"
#include "third_party/blink/renderer/core/testing/file_backed_blob_factory_test_helper.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {
namespace {

v8::Local<v8::Value> RoundTrip(v8::Local<v8::Value> value,
                               V8TestingScope& scope,
                               ExceptionState& exception_state,
                               Transferables* transferables = nullptr,
                               WebBlobInfoArray* blob_info = nullptr) {
  ScriptState* script_state = scope.GetScriptState();

  // Extract message ports and disentangle them.
  Vector<MessagePortChannel> channels;
  if (transferables) {
    channels = MessagePort::DisentanglePorts(scope.GetExecutionContext(),
                                             transferables->message_ports,
                                             exception_state);
    if (exception_state.HadException())
      return v8::Local<v8::Value>();
  }

  V8ScriptValueSerializer::Options serialize_options;
  serialize_options.transferables = transferables;
  serialize_options.blob_info = blob_info;
  V8ScriptValueSerializer serializer(script_state, serialize_options);
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      serializer.Serialize(value, exception_state);
  DCHECK_EQ(!serialized_script_value, exception_state.HadException());
  if (!serialized_script_value)
    return v8::Local<v8::Value>();
  // If there are message ports, make new ones and entangle them.
  MessagePortArray* transferred_message_ports = MessagePort::EntanglePorts(
      *scope.GetExecutionContext(), std::move(channels));

  UnpackedSerializedScriptValue* unpacked =
      SerializedScriptValue::Unpack(std::move(serialized_script_value));
  V8ScriptValueDeserializer::Options deserialize_options;
  deserialize_options.message_ports = transferred_message_ports;
  deserialize_options.blob_info = blob_info;
  V8ScriptValueDeserializer deserializer(script_state, unpacked,
                                         deserialize_options);
  return deserializer.Deserialize();
}

v8::Local<v8::Value> Eval(const String& source, V8TestingScope& scope) {
  return ClassicScript::CreateUnspecifiedScript(source)
      ->RunScriptAndReturnValue(&scope.GetWindow())
      .GetSuccessValueOrEmpty();
}

String ToJSON(v8::Local<v8::Object> object, const V8TestingScope& scope) {
  return ToBlinkString<String>(
      scope.GetIsolate(),
      v8::JSON::Stringify(scope.GetContext(), object).ToLocalChecked(),
      kDoNotExternalize);
}
}  // namespace

scoped_refptr<SerializedScriptValue> SerializedValue(
    const Vector<uint8_t>& bytes) {
  return SerializedScriptValue::Create(bytes);
}

// Checks for a DOM exception, including a rethrown one.
testing::AssertionResult HadDOMExceptionInCoreTest(const StringView& name,
                                                   ScriptState* script_state,
                                                   v8::TryCatch& try_catch) {
  if (!try_catch.HasCaught()) {
    return testing::AssertionFailure() << "no exception thrown";
  }
  DOMException* dom_exception = V8DOMException::ToWrappable(
      script_state->GetIsolate(), try_catch.Exception());
  if (!dom_exception)
    return testing::AssertionFailure()
           << "exception thrown was not a DOMException";
  if (dom_exception->name() != name)
    return testing::AssertionFailure() << "was " << dom_exception->name();
  return testing::AssertionSuccess();
}

TEST(V8ScriptValueSerializerTest, RoundTripJSONLikeValue) {
  test::TaskEnvironment task_environment;
  // Ensure that simple JavaScript objects work.
  // There are more exhaustive tests of JavaScript objects in V8.
  V8TestingScope scope;
  v8::Local<v8::Value> object = Eval("({ foo: [1, 2, 3], bar: 'baz' })", scope);
  DCHECK(object->IsObject());
  v8::Local<v8::Value> result =
      RoundTrip(object, scope, scope.GetExceptionState());
  ASSERT_TRUE(result->IsObject());
  EXPECT_NE(object, result);
  EXPECT_EQ(ToJSON(object.As<v8::Object>(), scope),
            ToJSON(result.As<v8::Object>(), scope));
}

TEST(V8ScriptValueSerializerTest, ThrowsDataCloneError) {
  test::TaskEnvironment task_environment;
  // Ensure that a proper DataCloneError DOMException is thrown when issues
  // are encountered in V8 (for example, cloning a symbol). It should be an
  // instance of DOMException.
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Value> symbol = Eval("Symbol()", scope);
  DCHECK(symbol->IsSymbol());
  ASSERT_FALSE(
      V8ScriptValueSerializer(script_state)
          .Serialize(symbol, PassThroughException(scope.GetIsolate())));
  ASSERT_TRUE(
      HadDOMExceptionInCoreTest("DataCloneError", script_state, try_catch));
  DOMException* dom_exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), try_catch.Exception());
  EXPECT_TRUE(dom_exception);
}

TEST(V8ScriptValueSerializerTest, RethrowsScriptError) {
  test::TaskEnvironment task_environment;
  // Ensure that other exceptions, like those thrown by script, are properly
  // rethrown.
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  v8::TryCatch try_catch(scope.GetIsolate());
  v8::Local<v8::Value> exception = Eval("myException=new Error()", scope);
  v8::Local<v8::Value> object =
      Eval("({ get a() { throw myException; }})", scope);
  DCHECK(object->IsObject());
  ASSERT_FALSE(
      V8ScriptValueSerializer(script_state)
          .Serialize(object, PassThroughException(scope.GetIsolate())));
  ASSERT_TRUE(try_catch.HasCaught());
  EXPECT_EQ(exception, try_catch.Exception());
}

TEST(V8ScriptValueSerializerTest, DeserializationErrorReturnsNull) {
  test::TaskEnvironment task_environment;
  // If there's a problem during deserialization, it results in null, but no
  // exception.
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> invalid =
      SerializedScriptValue::Create("invalid data");
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, invalid).Deserialize();
  EXPECT_TRUE(result->IsNull());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(V8ScriptValueSerializerTest, DetachHappensAfterSerialization) {
  test::TaskEnvironment task_environment;
  // This object will throw an exception before the [[Transfer]] step.
  // As a result, the ArrayBuffer will not be transferred.
  V8TestingScope scope;
  v8::TryCatch try_catch(scope.GetIsolate());

  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(1, 1);
  ASSERT_FALSE(array_buffer->IsDetached());
  v8::Local<v8::Value> object = Eval("({ get a() { throw 'party'; }})", scope);
  Transferables transferables;
  transferables.array_buffers.push_back(array_buffer);

  RoundTrip(object, scope, PassThroughException(scope.GetIsolate()),
            &transferables);
  ASSERT_TRUE(try_catch.HasCaught());
  EXPECT_FALSE(HadDOMExceptionInCoreTest("DataCloneError",
                                         scope.GetScriptState(), try_catch));
  EXPECT_FALSE(array_buffer->IsDetached());
}

TEST(V8ScriptValueSerializerTest, RoundTripDOMPoint) {
  test::TaskEnvironment task_environment;
  // DOMPoint objects should serialize and deserialize correctly.
  V8TestingScope scope;
  DOMPoint* point = DOMPoint::Create(1, 2, 3, 4);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMPoint>::ToV8(scope.GetScriptState(), point);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  DOMPoint* new_point = V8DOMPoint::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_point, nullptr);
  EXPECT_NE(point, new_point);
  EXPECT_EQ(point->x(), new_point->x());
  EXPECT_EQ(point->y(), new_point->y());
  EXPECT_EQ(point->z(), new_point->z());
  EXPECT_EQ(point->w(), new_point->w());
}

TEST(V8ScriptValueSerializerTest, DecodeDOMPoint) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x11, 0xff, 0x0d, 0x5c, 'Q',  0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x40});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  DOMPoint* point = V8DOMPoint::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(point, nullptr);
  EXPECT_EQ(1, point->x());
  EXPECT_EQ(2, point->y());
  EXPECT_EQ(3, point->z());
  EXPECT_EQ(4, point->w());
}

TEST(V8ScriptValueSerializerTest, RoundTripDOMPointReadOnly) {
  test::TaskEnvironment task_environment;
  // DOMPointReadOnly objects should serialize and deserialize correctly.
  V8TestingScope scope;
  DOMPointReadOnly* point = DOMPointReadOnly::Create(1, 2, 3, 4);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMPointReadOnly>::ToV8(scope.GetScriptState(), point);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  EXPECT_FALSE(V8DOMPoint::HasInstance(scope.GetIsolate(), result));
  DOMPointReadOnly* new_point =
      V8DOMPointReadOnly::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_point, nullptr);
  EXPECT_NE(point, new_point);
  EXPECT_EQ(point->x(), new_point->x());
  EXPECT_EQ(point->y(), new_point->y());
  EXPECT_EQ(point->z(), new_point->z());
  EXPECT_EQ(point->w(), new_point->w());
}

TEST(V8ScriptValueSerializerTest, DecodeDOMPointReadOnly) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x11, 0xff, 0x0d, 0x5c, 'W',  0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x40});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  DOMPointReadOnly* point =
      V8DOMPointReadOnly::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(point, nullptr);
  EXPECT_EQ(1, point->x());
  EXPECT_EQ(2, point->y());
  EXPECT_EQ(3, point->z());
  EXPECT_EQ(4, point->w());
}

TEST(V8ScriptValueSerializerTest, RoundTripDOMRect) {
  test::TaskEnvironment task_environment;
  // DOMRect objects should serialize and deserialize correctly.
  V8TestingScope scope;
  DOMRect* rect = DOMRect::Create(1, 2, 3, 4);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMRect>::ToV8(scope.GetScriptState(), rect);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  DOMRect* new_rect = V8DOMRect::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_rect, nullptr);
  EXPECT_NE(rect, new_rect);
  EXPECT_EQ(rect->x(), new_rect->x());
  EXPECT_EQ(rect->y(), new_rect->y());
  EXPECT_EQ(rect->width(), new_rect->width());
  EXPECT_EQ(rect->height(), new_rect->height());
}

TEST(V8ScriptValueSerializerTest, DecodeDOMRect) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x11, 0xff, 0x0d, 0x5c, 'E',  0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x40});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  DOMRect* rect = V8DOMRect::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(rect, nullptr);
  EXPECT_EQ(1, rect->x());
  EXPECT_EQ(2, rect->y());
  EXPECT_EQ(3, rect->width());
  EXPECT_EQ(4, rect->height());
}

TEST(V8ScriptValueSerializerTest, RoundTripDOMRectReadOnly) {
  test::TaskEnvironment task_environment;
  // DOMRectReadOnly objects should serialize and deserialize correctly.
  V8TestingScope scope;
  DOMRectReadOnly* rect = DOMRectReadOnly::Create(1, 2, 3, 4);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMRectReadOnly>::ToV8(scope.GetScriptState(), rect);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  EXPECT_FALSE(V8DOMRect::HasInstance(scope.GetIsolate(), result));
  DOMRectReadOnly* new_rect =
      V8DOMRectReadOnly::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_rect, nullptr);
  EXPECT_NE(rect, new_rect);
  EXPECT_EQ(rect->x(), new_rect->x());
  EXPECT_EQ(rect->y(), new_rect->y());
  EXPECT_EQ(rect->width(), new_rect->width());
  EXPECT_EQ(rect->height(), new_rect->height());
}

TEST(V8ScriptValueSerializerTest, DecodeDOMRectReadOnly) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x11, 0xff, 0x0d, 0x5c, 'R',  0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x40});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  DOMRectReadOnly* rect =
      V8DOMRectReadOnly::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(rect, nullptr);
  EXPECT_EQ(1, rect->x());
  EXPECT_EQ(2, rect->y());
  EXPECT_EQ(3, rect->width());
  EXPECT_EQ(4, rect->height());
}

TEST(V8ScriptValueSerializerTest, RoundTripDOMQuad) {
  test::TaskEnvironment task_environment;
  // DOMQuad objects should serialize and deserialize correctly.
  V8TestingScope scope;
  DOMPointInit* pi1 = DOMPointInit::Create();
  pi1->setX(1);
  pi1->setY(5);
  pi1->setZ(9);
  pi1->setW(13);
  DOMPointInit* pi2 = DOMPointInit::Create();
  pi2->setX(2);
  pi2->setY(6);
  pi2->setZ(10);
  pi2->setW(14);
  DOMPointInit* pi3 = DOMPointInit::Create();
  pi3->setX(3);
  pi3->setY(7);
  pi3->setZ(11);
  pi3->setW(15);
  DOMPointInit* pi4 = DOMPointInit::Create();
  pi4->setX(4);
  pi4->setY(8);
  pi4->setZ(12);
  pi4->setW(16);
  DOMQuad* quad = DOMQuad::Create(pi1, pi2, pi3, pi4);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMQuad>::ToV8(scope.GetScriptState(), quad);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  DOMQuad* new_quad = V8DOMQuad::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_quad, nullptr);
  EXPECT_NE(quad, new_quad);
  EXPECT_NE(quad->p1(), new_quad->p1());
  EXPECT_NE(quad->p2(), new_quad->p2());
  EXPECT_NE(quad->p3(), new_quad->p3());
  EXPECT_NE(quad->p4(), new_quad->p4());
  EXPECT_EQ(quad->p1()->x(), new_quad->p1()->x());
  EXPECT_EQ(quad->p1()->y(), new_quad->p1()->y());
  EXPECT_EQ(quad->p1()->z(), new_quad->p1()->z());
  EXPECT_EQ(quad->p1()->w(), new_quad->p1()->w());
  EXPECT_EQ(quad->p2()->x(), new_quad->p2()->x());
  EXPECT_EQ(quad->p2()->y(), new_quad->p2()->y());
  EXPECT_EQ(quad->p2()->z(), new_quad->p2()->z());
  EXPECT_EQ(quad->p2()->w(), new_quad->p2()->w());
  EXPECT_EQ(quad->p3()->x(), new_quad->p3()->x());
  EXPECT_EQ(quad->p3()->y(), new_quad->p3()->y());
  EXPECT_EQ(quad->p3()->z(), new_quad->p3()->z());
  EXPECT_EQ(quad->p3()->w(), new_quad->p3()->w());
  EXPECT_EQ(quad->p4()->x(), new_quad->p4()->x());
  EXPECT_EQ(quad->p4()->y(), new_quad->p4()->y());
  EXPECT_EQ(quad->p4()->z(), new_quad->p4()->z());
  EXPECT_EQ(quad->p4()->w(), new_quad->p4()->w());
}

TEST(V8ScriptValueSerializerTest, DecodeDOMQuad) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x11, 0xff, 0x0d, 0x5c, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x40, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x22, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x2a, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x18, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x24, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x2c, 0x40, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x08, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x1c, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x26, 0x40, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x2e, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x10, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x40, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x28, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x30, 0x40});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  DOMQuad* quad = V8DOMQuad::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(quad, nullptr);
  EXPECT_EQ(1, quad->p1()->x());
  EXPECT_EQ(5, quad->p1()->y());
  EXPECT_EQ(9, quad->p1()->z());
  EXPECT_EQ(13, quad->p1()->w());
  EXPECT_EQ(2, quad->p2()->x());
  EXPECT_EQ(6, quad->p2()->y());
  EXPECT_EQ(10, quad->p2()->z());
  EXPECT_EQ(14, quad->p2()->w());
  EXPECT_EQ(3, quad->p3()->x());
  EXPECT_EQ(7, quad->p3()->y());
  EXPECT_EQ(11, quad->p3()->z());
  EXPECT_EQ(15, quad->p3()->w());
  EXPECT_EQ(4, quad->p4()->x());
  EXPECT_EQ(8, quad->p4()->y());
  EXPECT_EQ(12, quad->p4()->z());
  EXPECT_EQ(16, quad->p4()->w());
}

TEST(V8ScriptValueSerializerTest, RoundTripDOMMatrix2D) {
  test::TaskEnvironment task_environment;
  // DOMMatrix objects should serialize and deserialize correctly.
  V8TestingScope scope;
  DOMMatrixInit* init = DOMMatrixInit::Create();
  init->setIs2D(true);
  init->setA(1.0);
  init->setB(2.0);
  init->setC(3.0);
  init->setD(4.0);
  init->setE(5.0);
  init->setF(6.0);
  DOMMatrix* matrix = DOMMatrix::fromMatrix(init, scope.GetExceptionState());
  EXPECT_TRUE(matrix->is2D());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMMatrix>::ToV8(scope.GetScriptState(), matrix);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  DOMMatrix* new_matrix = V8DOMMatrix::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_matrix, nullptr);
  EXPECT_NE(matrix, new_matrix);
  EXPECT_TRUE(new_matrix->is2D());
  EXPECT_EQ(matrix->a(), new_matrix->a());
  EXPECT_EQ(matrix->b(), new_matrix->b());
  EXPECT_EQ(matrix->c(), new_matrix->c());
  EXPECT_EQ(matrix->d(), new_matrix->d());
  EXPECT_EQ(matrix->e(), new_matrix->e());
  EXPECT_EQ(matrix->f(), new_matrix->f());
}

TEST(V8ScriptValueSerializerTest, DecodeDOMMatrix2D) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue({
      0xff, 0x11, 0xff, 0x0d, 0x5c, 0x49, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x08, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x40, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x18, 0x40, 0xff, 0x11, 0xff, 0x0d, 0x5c, 0x49,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x40, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x14, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x40,
  });
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  DOMMatrix* matrix = V8DOMMatrix::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(matrix, nullptr);
  EXPECT_TRUE(matrix->is2D());
  EXPECT_EQ(1.0, matrix->a());
  EXPECT_EQ(2.0, matrix->b());
  EXPECT_EQ(3.0, matrix->c());
  EXPECT_EQ(4.0, matrix->d());
  EXPECT_EQ(5.0, matrix->e());
  EXPECT_EQ(6.0, matrix->f());
}

TEST(V8ScriptValueSerializerTest, RoundTripDOMMatrixReadOnly2D) {
  test::TaskEnvironment task_environment;
  // DOMMatrix objects should serialize and deserialize correctly.
  V8TestingScope scope;
  DOMMatrixInit* init = DOMMatrixInit::Create();
  init->setIs2D(true);
  init->setA(1.0);
  init->setB(2.0);
  init->setC(3.0);
  init->setD(4.0);
  init->setE(5.0);
  init->setF(6.0);
  DOMMatrixReadOnly* matrix =
      DOMMatrixReadOnly::fromMatrix(init, scope.GetExceptionState());
  EXPECT_TRUE(matrix->is2D());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMMatrixReadOnly>::ToV8(scope.GetScriptState(), matrix);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  EXPECT_FALSE(V8DOMMatrix::HasInstance(scope.GetIsolate(), result));
  DOMMatrixReadOnly* new_matrix =
      V8DOMMatrixReadOnly::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_matrix, nullptr);
  EXPECT_NE(matrix, new_matrix);
  EXPECT_TRUE(new_matrix->is2D());
  EXPECT_EQ(matrix->a(), new_matrix->a());
  EXPECT_EQ(matrix->b(), new_matrix->b());
  EXPECT_EQ(matrix->c(), new_matrix->c());
  EXPECT_EQ(matrix->d(), new_matrix->d());
  EXPECT_EQ(matrix->e(), new_matrix->e());
  EXPECT_EQ(matrix->f(), new_matrix->f());
}

TEST(V8ScriptValueSerializerTest, DecodeDOMMatrixReadOnly2D) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue({
      0xff, 0x11, 0xff, 0x0d, 0x5c, 0x4f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x08, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x10, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x14, 0x40, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x18, 0x40, 0xff, 0x11, 0xff, 0x0d, 0x5c, 0x49,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x40,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x40, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x14, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x40,
  });
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  DOMMatrixReadOnly* matrix =
      V8DOMMatrixReadOnly::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(matrix, nullptr);
  EXPECT_TRUE(matrix->is2D());
  EXPECT_EQ(1.0, matrix->a());
  EXPECT_EQ(2.0, matrix->b());
  EXPECT_EQ(3.0, matrix->c());
  EXPECT_EQ(4.0, matrix->d());
  EXPECT_EQ(5.0, matrix->e());
  EXPECT_EQ(6.0, matrix->f());
}

TEST(V8ScriptValueSerializerTest, RoundTripDOMMatrix) {
  test::TaskEnvironment task_environment;
  // DOMMatrix objects should serialize and deserialize correctly.
  V8TestingScope scope;
  DOMMatrixInit* init = DOMMatrixInit::Create();
  init->setIs2D(false);
  init->setM11(1.1);
  init->setM12(1.2);
  init->setM13(1.3);
  init->setM14(1.4);
  init->setM21(2.1);
  init->setM22(2.2);
  init->setM23(2.3);
  init->setM24(2.4);
  init->setM31(3.1);
  init->setM32(3.2);
  init->setM33(3.3);
  init->setM34(3.4);
  init->setM41(4.1);
  init->setM42(4.2);
  init->setM43(4.3);
  init->setM44(4.4);
  DOMMatrix* matrix = DOMMatrix::fromMatrix(init, scope.GetExceptionState());
  EXPECT_FALSE(matrix->is2D());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMMatrix>::ToV8(scope.GetScriptState(), matrix);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  DOMMatrix* new_matrix = V8DOMMatrix::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_matrix, nullptr);
  EXPECT_NE(matrix, new_matrix);
  EXPECT_FALSE(new_matrix->is2D());
  EXPECT_EQ(matrix->m11(), new_matrix->m11());
  EXPECT_EQ(matrix->m12(), new_matrix->m12());
  EXPECT_EQ(matrix->m13(), new_matrix->m13());
  EXPECT_EQ(matrix->m14(), new_matrix->m14());
  EXPECT_EQ(matrix->m21(), new_matrix->m21());
  EXPECT_EQ(matrix->m22(), new_matrix->m22());
  EXPECT_EQ(matrix->m23(), new_matrix->m23());
  EXPECT_EQ(matrix->m24(), new_matrix->m24());
  EXPECT_EQ(matrix->m31(), new_matrix->m31());
  EXPECT_EQ(matrix->m32(), new_matrix->m32());
  EXPECT_EQ(matrix->m33(), new_matrix->m33());
  EXPECT_EQ(matrix->m34(), new_matrix->m34());
  EXPECT_EQ(matrix->m41(), new_matrix->m41());
  EXPECT_EQ(matrix->m42(), new_matrix->m42());
  EXPECT_EQ(matrix->m43(), new_matrix->m43());
  EXPECT_EQ(matrix->m44(), new_matrix->m44());
}

TEST(V8ScriptValueSerializerTest, DecodeDOMMatrix) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue({
      0xff, 0x11, 0xff, 0x0d, 0x5c, 0x59, 0x9a, 0x99, 0x99, 0x99, 0x99, 0x99,
      0xf1, 0x3f, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f, 0xcd, 0xcc,
      0xcc, 0xcc, 0xcc, 0xcc, 0xf4, 0x3f, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
      0xf6, 0x3f, 0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x00, 0x40, 0x9a, 0x99,
      0x99, 0x99, 0x99, 0x99, 0x01, 0x40, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
      0x02, 0x40, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x03, 0x40, 0xcd, 0xcc,
      0xcc, 0xcc, 0xcc, 0xcc, 0x08, 0x40, 0x9a, 0x99, 0x99, 0x99, 0x99, 0x99,
      0x09, 0x40, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x0a, 0x40, 0x33, 0x33,
      0x33, 0x33, 0x33, 0x33, 0x0b, 0x40, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
      0x10, 0x40, 0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0x40, 0x33, 0x33,
      0x33, 0x33, 0x33, 0x33, 0x11, 0x40, 0x9a, 0x99, 0x99, 0x99, 0x99, 0x99,
      0x11, 0x40,
  });
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  DOMMatrix* matrix = V8DOMMatrix::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(matrix, nullptr);
  EXPECT_FALSE(matrix->is2D());
  EXPECT_EQ(1.1, matrix->m11());
  EXPECT_EQ(1.2, matrix->m12());
  EXPECT_EQ(1.3, matrix->m13());
  EXPECT_EQ(1.4, matrix->m14());
  EXPECT_EQ(2.1, matrix->m21());
  EXPECT_EQ(2.2, matrix->m22());
  EXPECT_EQ(2.3, matrix->m23());
  EXPECT_EQ(2.4, matrix->m24());
  EXPECT_EQ(3.1, matrix->m31());
  EXPECT_EQ(3.2, matrix->m32());
  EXPECT_EQ(3.3, matrix->m33());
  EXPECT_EQ(3.4, matrix->m34());
  EXPECT_EQ(4.1, matrix->m41());
  EXPECT_EQ(4.2, matrix->m42());
  EXPECT_EQ(4.3, matrix->m43());
  EXPECT_EQ(4.4, matrix->m44());
}

TEST(V8ScriptValueSerializerTest, RoundTripDOMMatrixReadOnly) {
  test::TaskEnvironment task_environment;
  // DOMMatrixReadOnly objects should serialize and deserialize correctly.
  V8TestingScope scope;
  DOMMatrixInit* init = DOMMatrixInit::Create();
  init->setIs2D(false);
  init->setM11(1.1);
  init->setM12(1.2);
  init->setM13(1.3);
  init->setM14(1.4);
  init->setM21(2.1);
  init->setM22(2.2);
  init->setM23(2.3);
  init->setM24(2.4);
  init->setM31(3.1);
  init->setM32(3.2);
  init->setM33(3.3);
  init->setM34(3.4);
  init->setM41(4.1);
  init->setM42(4.2);
  init->setM43(4.3);
  init->setM44(4.4);
  DOMMatrixReadOnly* matrix =
      DOMMatrixReadOnly::fromMatrix(init, scope.GetExceptionState());
  EXPECT_FALSE(matrix->is2D());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMMatrixReadOnly>::ToV8(scope.GetScriptState(), matrix);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  EXPECT_FALSE(V8DOMMatrix::HasInstance(scope.GetIsolate(), result));
  DOMMatrixReadOnly* new_matrix =
      V8DOMMatrixReadOnly::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_matrix, nullptr);
  EXPECT_NE(matrix, new_matrix);
  EXPECT_FALSE(new_matrix->is2D());
  EXPECT_EQ(matrix->m11(), new_matrix->m11());
  EXPECT_EQ(matrix->m12(), new_matrix->m12());
  EXPECT_EQ(matrix->m13(), new_matrix->m13());
  EXPECT_EQ(matrix->m14(), new_matrix->m14());
  EXPECT_EQ(matrix->m21(), new_matrix->m21());
  EXPECT_EQ(matrix->m22(), new_matrix->m22());
  EXPECT_EQ(matrix->m23(), new_matrix->m23());
  EXPECT_EQ(matrix->m24(), new_matrix->m24());
  EXPECT_EQ(matrix->m31(), new_matrix->m31());
  EXPECT_EQ(matrix->m32(), new_matrix->m32());
  EXPECT_EQ(matrix->m33(), new_matrix->m33());
  EXPECT_EQ(matrix->m34(), new_matrix->m34());
  EXPECT_EQ(matrix->m41(), new_matrix->m41());
  EXPECT_EQ(matrix->m42(), new_matrix->m42());
  EXPECT_EQ(matrix->m43(), new_matrix->m43());
  EXPECT_EQ(matrix->m44(), new_matrix->m44());
}

TEST(V8ScriptValueSerializerTest, DecodeDOMMatrixReadOnly) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue({
      0xff, 0x11, 0xff, 0x0d, 0x5c, 0x55, 0x9a, 0x99, 0x99, 0x99, 0x99, 0x99,
      0xf1, 0x3f, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0xf3, 0x3f, 0xcd, 0xcc,
      0xcc, 0xcc, 0xcc, 0xcc, 0xf4, 0x3f, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
      0xf6, 0x3f, 0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x00, 0x40, 0x9a, 0x99,
      0x99, 0x99, 0x99, 0x99, 0x01, 0x40, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
      0x02, 0x40, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x03, 0x40, 0xcd, 0xcc,
      0xcc, 0xcc, 0xcc, 0xcc, 0x08, 0x40, 0x9a, 0x99, 0x99, 0x99, 0x99, 0x99,
      0x09, 0x40, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x0a, 0x40, 0x33, 0x33,
      0x33, 0x33, 0x33, 0x33, 0x0b, 0x40, 0x66, 0x66, 0x66, 0x66, 0x66, 0x66,
      0x10, 0x40, 0xcd, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0x40, 0x33, 0x33,
      0x33, 0x33, 0x33, 0x33, 0x11, 0x40, 0x9a, 0x99, 0x99, 0x99, 0x99, 0x99,
      0x11, 0x40,

  });
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  DOMMatrixReadOnly* matrix =
      V8DOMMatrixReadOnly::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(matrix, nullptr);
  EXPECT_FALSE(matrix->is2D());
  EXPECT_EQ(1.1, matrix->m11());
  EXPECT_EQ(1.2, matrix->m12());
  EXPECT_EQ(1.3, matrix->m13());
  EXPECT_EQ(1.4, matrix->m14());
  EXPECT_EQ(2.1, matrix->m21());
  EXPECT_EQ(2.2, matrix->m22());
  EXPECT_EQ(2.3, matrix->m23());
  EXPECT_EQ(2.4, matrix->m24());
  EXPECT_EQ(3.1, matrix->m31());
  EXPECT_EQ(3.2, matrix->m32());
  EXPECT_EQ(3.3, matrix->m33());
  EXPECT_EQ(3.4, matrix->m34());
  EXPECT_EQ(4.1, matrix->m41());
  EXPECT_EQ(4.2, matrix->m42());
  EXPECT_EQ(4.3, matrix->m43());
  EXPECT_EQ(4.4, matrix->m44());
}

TEST(V8ScriptValueSerializerTest, RoundTripImageData) {
  test::TaskEnvironment task_environment;
  // ImageData objects should serialize and deserialize correctly.
  V8TestingScope scope;
  ImageData* image_data = ImageData::ValidateAndCreate(
      2, 1, std::nullopt, nullptr, ImageData::ValidateAndCreateParams(),
      ASSERT_NO_EXCEPTION);
  SkPixmap pm = image_data->GetSkPixmap();
  pm.writable_addr32(0, 0)[0] = 200u;
  pm.writable_addr32(1, 0)[0] = 100u;
  v8::Local<v8::Value> wrapper =
      ToV8Traits<ImageData>::ToV8(scope.GetScriptState(), image_data);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  ImageData* new_image_data =
      V8ImageData::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_data, nullptr);
  EXPECT_NE(image_data, new_image_data);
  EXPECT_EQ(image_data->Size(), new_image_data->Size());
  SkPixmap new_pm = new_image_data->GetSkPixmap();
  EXPECT_EQ(200u, new_pm.addr32(0, 0)[0]);
  EXPECT_EQ(100u, new_pm.addr32(1, 0)[0]);
}

TEST(V8ScriptValueSerializerTest, RoundTripDetachedImageData) {
  test::TaskEnvironment task_environment;
  // If an ImageData is detached, it can be serialized, but will fail when being
  // deserialized.
  V8TestingScope scope;
  ImageData* image_data = ImageData::ValidateAndCreate(
      2, 1, std::nullopt, nullptr, ImageData::ValidateAndCreateParams(),
      ASSERT_NO_EXCEPTION);
  SkPixmap pm = image_data->GetSkPixmap();
  pm.writable_addr32(0, 0)[0] = 200u;
  image_data->data()->GetAsUint8ClampedArray()->BufferBase()->Detach();

  v8::Local<v8::Value> wrapper =
      ToV8Traits<ImageData>::ToV8(scope.GetScriptState(), image_data);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  EXPECT_FALSE(V8ImageData::HasInstance(scope.GetIsolate(), result));
}

TEST(V8ScriptValueSerializerTest, RoundTripImageDataWithColorSpaceInfo) {
  test::TaskEnvironment task_environment;
  // ImageData objects with color space information should serialize and
  // deserialize correctly.
  V8TestingScope scope;
  ImageDataSettings* image_data_settings = ImageDataSettings::Create();
  image_data_settings->setColorSpace("display-p3");
  image_data_settings->setStorageFormat("float32");
  ImageData* image_data = ImageData::ValidateAndCreate(
      2, 1, std::nullopt, image_data_settings,
      ImageData::ValidateAndCreateParams(), ASSERT_NO_EXCEPTION);
  SkPixmap pm = image_data->GetSkPixmap();
  EXPECT_EQ(kRGBA_F32_SkColorType, pm.info().colorType());
  static_cast<float*>(pm.writable_addr(0, 0))[0] = 200.f;

  v8::Local<v8::Value> wrapper =
      ToV8Traits<ImageData>::ToV8(scope.GetScriptState(), image_data);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  ImageData* new_image_data =
      V8ImageData::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_data, nullptr);
  EXPECT_NE(image_data, new_image_data);
  EXPECT_EQ(image_data->Size(), new_image_data->Size());
  ImageDataSettings* new_image_data_settings = new_image_data->getSettings();
  EXPECT_EQ("display-p3", new_image_data_settings->colorSpace());
  EXPECT_EQ("float32", new_image_data_settings->storageFormat());
  SkPixmap new_pm = new_image_data->GetSkPixmap();
  EXPECT_EQ(kRGBA_F32_SkColorType, new_pm.info().colorType());
  EXPECT_EQ(200.f, reinterpret_cast<const float*>(new_pm.addr(0, 0))[0]);
}

TEST(V8ScriptValueSerializerTest, DecodeImageDataV9) {
  test::TaskEnvironment task_environment;
  // Backward compatibility with existing serialized ImageData objects must be
  // maintained. Add more cases if the format changes; don't remove tests for
  // old versions.
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x23, 0x02, 0x01, 0x08, 0xc8,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  ImageData* new_image_data =
      V8ImageData::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_data, nullptr);
  EXPECT_EQ(gfx::Size(2, 1), new_image_data->Size());
  SkPixmap new_pm = new_image_data->GetSkPixmap();
  EXPECT_EQ(8u, new_pm.computeByteSize());
  EXPECT_EQ(200u, new_pm.addr32()[0]);
}

TEST(V8ScriptValueSerializerTest, DecodeImageDataV16) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x10, 0xff, 0x0c, 0x23, 0x02, 0x01, 0x08, 0xc8,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  ImageData* new_image_data =
      V8ImageData::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_data, nullptr);
  EXPECT_EQ(gfx::Size(2, 1), new_image_data->Size());
  SkPixmap new_pm = new_image_data->GetSkPixmap();
  EXPECT_EQ(kRGBA_8888_SkColorType, new_pm.info().colorType());
  EXPECT_EQ(8u, new_pm.computeByteSize());
  EXPECT_EQ(200u, new_pm.addr32()[0]);
}

TEST(V8ScriptValueSerializerTest, DecodeImageDataV18) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x12, 0xff, 0x0d, 0x5c, 0x23, 0x01, 0x03, 0x03, 0x02, 0x00, 0x02,
       0x01, 0x20, 0xc8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  ImageData* new_image_data =
      V8ImageData::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_data, nullptr);
  EXPECT_EQ(gfx::Size(2, 1), new_image_data->Size());
  ImageDataSettings* new_image_data_settings = new_image_data->getSettings();
  EXPECT_EQ("display-p3", new_image_data_settings->colorSpace());
  EXPECT_EQ("float32", new_image_data_settings->storageFormat());
  SkPixmap new_pm = new_image_data->GetSkPixmap();
  EXPECT_EQ(kRGBA_F32_SkColorType, new_pm.info().colorType());
  EXPECT_EQ(200u, static_cast<const uint8_t*>(new_pm.addr(0, 0))[0]);
}

TEST(V8ScriptValueSerializerTest, InvalidImageDataDecodeV18) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  {
    // Nonsense image serialization tag (kOriginCleanTag).
    scoped_refptr<SerializedScriptValue> input =
        SerializedValue({0xff, 0x12, 0xff, 0x0d, 0x5c, 0x23, 0x02, 0x00, 0x00,
                         0x01, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
}

MessagePort* MakeMessagePort(ExecutionContext* execution_context,
                             ::MojoHandle* unowned_handle_out = nullptr) {
  auto* port = MakeGarbageCollected<MessagePort>(*execution_context);
  blink::MessagePortDescriptorPair pipe;
  ::MojoHandle unowned_handle = pipe.port0().handle().get().value();
  port->Entangle(pipe.TakePort0(), nullptr);
  EXPECT_TRUE(port->IsEntangled());
  EXPECT_EQ(unowned_handle, port->EntangledHandleForTesting());
  if (unowned_handle_out)
    *unowned_handle_out = unowned_handle;
  return port;
}

TEST(V8ScriptValueSerializerTest, RoundTripMessagePort) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  ::MojoHandle unowned_handle;
  MessagePort* port =
      MakeMessagePort(scope.GetExecutionContext(), &unowned_handle);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MessagePort>::ToV8(scope.GetScriptState(), port);
  Transferables transferables;
  transferables.message_ports.push_back(port);

  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState(), &transferables);
  MessagePort* new_port =
      V8MessagePort::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_port, nullptr);
  EXPECT_FALSE(port->IsEntangled());
  EXPECT_TRUE(new_port->IsEntangled());
  EXPECT_EQ(unowned_handle, new_port->EntangledHandleForTesting());
}

TEST(V8ScriptValueSerializerTest, NeuteredMessagePortThrowsDataCloneError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::TryCatch try_catch(scope.GetIsolate());

  auto* port = MakeGarbageCollected<MessagePort>(*scope.GetExecutionContext());
  EXPECT_TRUE(port->IsNeutered());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MessagePort>::ToV8(scope.GetScriptState(), port);
  Transferables transferables;
  transferables.message_ports.push_back(port);

  RoundTrip(wrapper, scope, PassThroughException(scope.GetIsolate()),
            &transferables);
  ASSERT_TRUE(HadDOMExceptionInCoreTest("DataCloneError",
                                        scope.GetScriptState(), try_catch));
}

TEST(V8ScriptValueSerializerTest,
     UntransferredMessagePortThrowsDataCloneError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::TryCatch try_catch(scope.GetIsolate());

  MessagePort* port = MakeMessagePort(scope.GetExecutionContext());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MessagePort>::ToV8(scope.GetScriptState(), port);
  Transferables transferables;

  RoundTrip(wrapper, scope, PassThroughException(scope.GetIsolate()),
            &transferables);
  ASSERT_TRUE(HadDOMExceptionInCoreTest("DataCloneError",
                                        scope.GetScriptState(), try_catch));
}

TEST(V8ScriptValueSerializerTest, OutOfRangeMessagePortIndex) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4d, 0x01});
  MessagePort* port1 = MakeMessagePort(scope.GetExecutionContext());
  MessagePort* port2 = MakeMessagePort(scope.GetExecutionContext());
  {
    V8ScriptValueDeserializer deserializer(script_state, input);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
  {
    V8ScriptValueDeserializer::Options options;
    options.message_ports = MakeGarbageCollected<MessagePortArray>();
    V8ScriptValueDeserializer deserializer(script_state, input, options);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
  {
    V8ScriptValueDeserializer::Options options;
    options.message_ports = MakeGarbageCollected<MessagePortArray>();
    options.message_ports->push_back(port1);
    V8ScriptValueDeserializer deserializer(script_state, input, options);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
  {
    V8ScriptValueDeserializer::Options options;
    options.message_ports = MakeGarbageCollected<MessagePortArray>();
    options.message_ports->push_back(port1);
    options.message_ports->push_back(port2);
    V8ScriptValueDeserializer deserializer(script_state, input, options);
    v8::Local<v8::Value> result = deserializer.Deserialize();
    EXPECT_EQ(port2, V8MessagePort::ToWrappable(scope.GetIsolate(), result));
  }
}

TEST(V8ScriptValueSerializerTest, RoundTripMojoHandle) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ContextFeatureSettings::From(
      scope.GetExecutionContext(),
      ContextFeatureSettings::CreationMode::kCreateIfNotExists)
      ->EnableMojoJS(true);

  mojo::MessagePipe pipe;
  auto* handle = MakeGarbageCollected<MojoHandle>(
      mojo::ScopedHandle::From(std::move(pipe.handle0)));
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MojoHandle>::ToV8(scope.GetScriptState(), handle);
  Transferables transferables;
  transferables.mojo_handles.push_back(handle);

  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState(), &transferables);
  MojoHandle* new_handle =
      V8MojoHandle::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_handle, nullptr);
  EXPECT_FALSE(handle->TakeHandle().is_valid());
  EXPECT_TRUE(new_handle->TakeHandle().is_valid());
}

TEST(V8ScriptValueSerializerTest, UntransferredMojoHandleThrowsDataCloneError) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::TryCatch try_catch(scope.GetIsolate());

  mojo::MessagePipe pipe;
  auto* handle = MakeGarbageCollected<MojoHandle>(
      mojo::ScopedHandle::From(std::move(pipe.handle0)));
  v8::Local<v8::Value> wrapper =
      ToV8Traits<MojoHandle>::ToV8(scope.GetScriptState(), handle);
  Transferables transferables;

  RoundTrip(wrapper, scope, PassThroughException(scope.GetIsolate()),
            &transferables);
  ASSERT_TRUE(HadDOMExceptionInCoreTest("DataCloneError",
                                        scope.GetScriptState(), try_catch));
}

// Decode tests for backward compatibility are not required for message ports
// and Mojo handles because they cannot be persisted to disk.

// A more exhaustive set of ImageBitmap cases are covered by web tests.
TEST(V8ScriptValueSerializerTest, RoundTripImageBitmap) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;

  // Make a 10x7 red ImageBitmap.
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(10, 7));
  surface->getCanvas()->clear(SK_ColorRED);
  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot()));
  ASSERT_TRUE(image_bitmap->BitmapImage());

  // Serialize and deserialize it.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<ImageBitmap>::ToV8(scope.GetScriptState(), image_bitmap);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  ImageBitmap* new_image_bitmap =
      V8ImageBitmap::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_bitmap, nullptr);
  ASSERT_TRUE(new_image_bitmap->BitmapImage());
  ASSERT_EQ(gfx::Size(10, 7), new_image_bitmap->Size());

  // Check that the pixel at (3, 3) is red.
  uint8_t pixel[4] = {};
  ASSERT_TRUE(
      new_image_bitmap->BitmapImage()->PaintImageForCurrentFrame().readPixels(
          SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
          &pixel, 4, 3, 3));
  ASSERT_THAT(pixel, testing::ElementsAre(255, 0, 0, 255));
}

TEST(V8ScriptValueSerializerTest, ImageBitmapEXIFImageOrientation) {
  test::TaskEnvironment task_environment;
  // More complete end-to-end testing is provided by WPT test
  // imagebitmap-replication-exif-orientation.html.
  // The purpose of this complementary test is to get complete code coverage
  // for all possible values of ImageOrientationEnum.
  V8TestingScope scope;
  const uint32_t kImageWidth = 10;
  const uint32_t kImageHeight = 5;
  for (uint8_t i = static_cast<uint8_t>(ImageOrientationEnum::kOriginTopLeft);
       i <= static_cast<uint8_t>(ImageOrientationEnum::kMaxValue); i++) {
    ImageOrientationEnum orientation = static_cast<ImageOrientationEnum>(i);
    sk_sp<SkSurface> surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(kImageWidth, kImageHeight));
    auto static_image =
        UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot());
    static_image->SetOrientation(orientation);
    auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(static_image);
    ASSERT_TRUE(image_bitmap->BitmapImage());
    // Serialize and deserialize it.
    v8::Local<v8::Value> wrapper =
        ToV8Traits<ImageBitmap>::ToV8(scope.GetScriptState(), image_bitmap);
    v8::Local<v8::Value> result =
        RoundTrip(wrapper, scope, scope.GetExceptionState());
    ImageBitmap* new_image_bitmap =
        V8ImageBitmap::ToWrappable(scope.GetIsolate(), result);
    ASSERT_NE(new_image_bitmap, nullptr);
    ASSERT_TRUE(new_image_bitmap->BitmapImage());
    // Ensure image orientation did not confuse (e.g transpose) the image size
    ASSERT_EQ(new_image_bitmap->Size(), image_bitmap->Size());
    ASSERT_EQ(new_image_bitmap->ImageOrientation(), orientation);
  }
}

TEST(V8ScriptValueSerializerTest, RoundTripImageBitmapWithColorSpaceInfo) {
  test::TaskEnvironment task_environment;
  sk_sp<SkColorSpace> p3 =
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kDisplayP3);
  V8TestingScope scope;
  // Make a 10x7 red ImageBitmap in P3 color space.
  SkImageInfo info =
      SkImageInfo::Make(10, 7, kRGBA_F16_SkColorType, kPremul_SkAlphaType, p3);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info);
  surface->getCanvas()->clear(SK_ColorRED);
  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot()));
  ASSERT_TRUE(image_bitmap->BitmapImage());

  // Serialize and deserialize it.
  v8::Local<v8::Value> wrapper =
      ToV8Traits<ImageBitmap>::ToV8(scope.GetScriptState(), image_bitmap);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  ImageBitmap* new_image_bitmap =
      V8ImageBitmap::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_bitmap, nullptr);
  ASSERT_TRUE(new_image_bitmap->BitmapImage());
  ASSERT_EQ(gfx::Size(10, 7), new_image_bitmap->Size());

  // Check the color settings.
  SkImageInfo bitmap_info = new_image_bitmap->GetBitmapSkImageInfo();
  EXPECT_EQ(kRGBA_F16_SkColorType, bitmap_info.colorType());
  EXPECT_TRUE(SkColorSpace::Equals(p3.get(), bitmap_info.colorSpace()));

  // Check that the pixel at (3, 3) is red. We expect red in P3 to be
  // {0x57, 0x3B, 0x68, 0x32, 0x6E, 0x30, 0x00, 0x3C} when each color
  // component is presented as a half float in Skia. However, difference in
  // GPU hardware may result in small differences in lower significant byte in
  // Skia color conversion pipeline. Hence, we use a tolerance of 2 here.
  uint8_t pixel[8] = {};
  ASSERT_TRUE(
      new_image_bitmap->BitmapImage()->PaintImageForCurrentFrame().readPixels(
          info.makeWH(1, 1), &pixel, 8, 3, 3));
  uint8_t p3_red[8] = {0x57, 0x3B, 0x68, 0x32, 0x6E, 0x30, 0x00, 0x3C};
  bool approximate_match = true;
  uint8_t tolerance = 2;
  for (int i = 0; i < 8; i++) {
    if (std::abs(p3_red[i] - pixel[i]) > tolerance) {
      approximate_match = false;
      break;
    }
  }
  ASSERT_TRUE(approximate_match);
}

TEST(V8ScriptValueSerializerTest, DecodeImageBitmap) {
  test::TaskEnvironment task_environment;
  // Backward compatibility with existing serialized ImageBitmap objects must be
  // maintained. Add more cases if the format changes; don't remove tests for
  // old versions.
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

// This is checked by platform instead of by SK_PMCOLOR_BYTE_ORDER because
// this test intends to ensure that a platform can decode images it has
// previously written. At format version 9, Android writes RGBA and every
// other platform writes BGRA.
#if BUILDFLAG(IS_ANDROID)
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
                       0x08, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff});
#else
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
                       0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
#endif

  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  ImageBitmap* new_image_bitmap =
      V8ImageBitmap::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_bitmap, nullptr);
  ASSERT_EQ(gfx::Size(2, 1), new_image_bitmap->Size());

  // Check that the pixels are opaque red and green, respectively.
  uint8_t pixels[8] = {};
  ASSERT_TRUE(
      new_image_bitmap->BitmapImage()->PaintImageForCurrentFrame().readPixels(
          SkImageInfo::Make(2, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
          &pixels, 8, 0, 0));
  ASSERT_THAT(pixels, testing::ElementsAre(255, 0, 0, 255, 0, 255, 0, 255));
  // Check that orientation is top left (default).
  ASSERT_EQ(new_image_bitmap->ImageOrientation(),
            ImageOrientationEnum::kOriginTopLeft);
}

TEST(V8ScriptValueSerializerTest, DecodeImageBitmapV18) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x12, 0xff, 0x0d, 0x5c, 0x67, 0x01, 0x03, 0x02, 0x01, 0x04, 0x01,
       0x05, 0x01, 0x00, 0x02, 0x01, 0x10, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24,
       0x00, 0x3c, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24, 0x00, 0x3c});
  sk_sp<SkColorSpace> p3 =
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kDisplayP3);

  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  ImageBitmap* new_image_bitmap =
      V8ImageBitmap::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_bitmap, nullptr);
  ASSERT_EQ(gfx::Size(2, 1), new_image_bitmap->Size());

  // Check the color settings.
  SkImageInfo bitmap_info = new_image_bitmap->GetBitmapSkImageInfo();
  EXPECT_EQ(kRGBA_F16_SkColorType, bitmap_info.colorType());
  EXPECT_TRUE(SkColorSpace::Equals(p3.get(), bitmap_info.colorSpace()));

  // Check that the pixel at (1, 0) is red.
  uint8_t pixel[8] = {};
  SkImageInfo info =
      SkImageInfo::Make(1, 1, kRGBA_F16_SkColorType, kPremul_SkAlphaType, p3);
  ASSERT_TRUE(
      new_image_bitmap->BitmapImage()->PaintImageForCurrentFrame().readPixels(
          info, &pixel, 8, 1, 0));
  // The reference values are the hex representation of red in P3 (as stored
  // in half floats by Skia).
  ASSERT_THAT(pixel, testing::ElementsAre(0x94, 0x3A, 0x3F, 0x28, 0x5F, 0x24,
                                          0x0, 0x3C));
  // Check that orientation is top left (default).
  ASSERT_EQ(new_image_bitmap->ImageOrientation(),
            ImageOrientationEnum::kOriginTopLeft);
}

TEST(V8ScriptValueSerializerTest, DecodeImageBitmapV20WithoutImageOrientation) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue({
      0xff,  // kVersionTag
      0x14,  // 20
      0xff,  // Value serializer header
      0x0f,  // Value serializer version 15 (varint format)
      0x5c,  // kHostObjectTag
      0x67,  // kImageBitmapTag
      0x07,  // kParametricColorSpaceTag
      // srgb colorspace
      0x00, 0x00, 0x00, 0x40, 0x33, 0x33, 0x03, 0x40, 0x00, 0x00, 0x00, 0xc0,
      0xed, 0x54, 0xee, 0x3f, 0x00, 0x00, 0x00, 0x20, 0x23, 0xb1, 0xaa, 0x3f,
      0x00, 0x00, 0x00, 0x20, 0x72, 0xd0, 0xb3, 0x3f, 0x00, 0x00, 0x00, 0xc0,
      0xdc, 0xb5, 0xa4, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x80, 0xe8, 0xdb, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x40, 0xa6, 0xd8, 0x3f,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0xc2, 0x3f, 0x00, 0x00, 0x00, 0x00,
      0x80, 0x7a, 0xcc, 0x3f, 0x00, 0x00, 0x00, 0x00, 0xa0, 0xf0, 0xe6, 0x3f,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0xaf, 0x3f, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x80, 0x8c, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0xda, 0xb8, 0x3f,
      0x00, 0x00, 0x00, 0x00, 0xe0, 0xd9, 0xe6, 0x3f, 0x02,
      0x03,        // kCanvasPixelFormatTag kBGRA8
      0x06, 0x00,  // kCanvasOpacityModeTag
      0x04, 0x01,  // kOriginCleanTag
      0x05, 0x01,  // kIsPremultipliedTag
      // Image orientation omitted
      0x0,         // kEndTag
      0x01, 0x01,  // width, height (varint format)
      0x04,        // pixel size (varint format)
      0xee, 0xaa, 0x77, 0xff,
      0x00  // padding: even number of bytes for endianness swapping.
  });

  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  ImageBitmap* new_image_bitmap =
      V8ImageBitmap::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_bitmap, nullptr);
  // Check image size
  ASSERT_EQ(gfx::Size(1, 1), new_image_bitmap->Size());
  // Check the color settings.
  SkImageInfo bitmap_info = new_image_bitmap->GetBitmapSkImageInfo();
  EXPECT_EQ(kBGRA_8888_SkColorType, bitmap_info.colorType());
  sk_sp<SkColorSpace> srgb =
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kSRGB);
  EXPECT_TRUE(SkColorSpace::Equals(srgb.get(), bitmap_info.colorSpace()));
  // Check that orientation is bottom left.
  ASSERT_EQ(new_image_bitmap->ImageOrientation(),
            ImageOrientationEnum::kOriginTopLeft);
  // Check pixel value
  SkImageInfo info = SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType,
                                       kPremul_SkAlphaType, srgb);
  uint8_t pixel[4] = {};
  ASSERT_TRUE(
      new_image_bitmap->BitmapImage()->PaintImageForCurrentFrame().readPixels(
          info, &pixel, 4, 0, 0));
  // BGRA encoding, swapped to RGBA
  ASSERT_THAT(pixel, testing::ElementsAre(0x77, 0xaa, 0xee, 0xff));
}

TEST(V8ScriptValueSerializerTest, DecodeImageBitmapV20WithImageOrientation) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  scoped_refptr<SerializedScriptValue> input = SerializedValue({
      0xff,  // kVersionTag
      0x14,  // 20
      0xff,  // Value serializer header
      0x0f,  // Value serializer version 15, varint encoding
      0x5c,  // kHostObjectTag
      0x67,  // kImageBitmapTag
      0x07,  // kParametricColorSpaceTag
      // srgb colorspace
      0x00, 0x00, 0x00, 0x40, 0x33, 0x33, 0x03, 0x40, 0x00, 0x00, 0x00, 0xc0,
      0xed, 0x54, 0xee, 0x3f, 0x00, 0x00, 0x00, 0x20, 0x23, 0xb1, 0xaa, 0x3f,
      0x00, 0x00, 0x00, 0x20, 0x72, 0xd0, 0xb3, 0x3f, 0x00, 0x00, 0x00, 0xc0,
      0xdc, 0xb5, 0xa4, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x80, 0xe8, 0xdb, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x40, 0xa6, 0xd8, 0x3f,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x50, 0xc2, 0x3f, 0x00, 0x00, 0x00, 0x00,
      0x80, 0x7a, 0xcc, 0x3f, 0x00, 0x00, 0x00, 0x00, 0xa0, 0xf0, 0xe6, 0x3f,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0xaf, 0x3f, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x80, 0x8c, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0xda, 0xb8, 0x3f,
      0x00, 0x00, 0x00, 0x00, 0xe0, 0xd9, 0xe6, 0x3f, 0x02,
      0x03,        // kCanvasPixelFormatTag kBGRA8
      0x06, 0x00,  // kCanvasOpacityModeTag
      0x04, 0x01,  // kOriginCleanTag
      0x05, 0x01,  // kIsPremultipliedTag
      0x08, 0x03,  // kImageOrientationTag -> kBottomLeft
      0x0,         // kEndTag
      0x01, 0x01,  // width, height (varint format)
      0x04,        // pixel size (varint format)
      0xee, 0xaa, 0x77, 0xff,
      0x00  // padding: even number of bytes for endianness swapping.
  });

  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  ImageBitmap* new_image_bitmap =
      V8ImageBitmap::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_bitmap, nullptr);
  // Check image size
  ASSERT_EQ(gfx::Size(1, 1), new_image_bitmap->Size());
  // Check the color settings.
  SkImageInfo bitmap_info = new_image_bitmap->GetBitmapSkImageInfo();
  EXPECT_EQ(kBGRA_8888_SkColorType, bitmap_info.colorType());
  sk_sp<SkColorSpace> srgb =
      SkColorSpace::MakeRGB(SkNamedTransferFn::kSRGB, SkNamedGamut::kSRGB);
  EXPECT_TRUE(SkColorSpace::Equals(srgb.get(), bitmap_info.colorSpace()));
  // Check that orientation is bottom left.
  ASSERT_EQ(new_image_bitmap->ImageOrientation(),
            ImageOrientationEnum::kOriginBottomLeft);
  // Check pixel value
  SkImageInfo info = SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType,
                                       kPremul_SkAlphaType, srgb);
  uint8_t pixel[4] = {};
  ASSERT_TRUE(
      new_image_bitmap->BitmapImage()->PaintImageForCurrentFrame().readPixels(
          info, &pixel, 4, 0, 0));
  // BGRA encoding, swapped to RGBA
  ASSERT_THAT(pixel, testing::ElementsAre(0x77, 0xaa, 0xee, 0xff));
}

TEST(V8ScriptValueSerializerTest, InvalidImageBitmapDecode) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  {
    // Too many bytes declared in pixel data.
    scoped_refptr<SerializedScriptValue> input = SerializedValue(
        {0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01, 0x09,
         0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Too few bytes declared in pixel data.
    scoped_refptr<SerializedScriptValue> input =
        SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
                         0x07, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Nonsense for origin clean data.
    scoped_refptr<SerializedScriptValue> input =
        SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x02, 0x01, 0x02, 0x01,
                         0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Nonsense for premultiplied bit.
    scoped_refptr<SerializedScriptValue> input =
        SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x02, 0x02, 0x01,
                         0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
}

TEST(V8ScriptValueSerializerTest, InvalidImageBitmapDecodeV18) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  {
    // Too many bytes declared in pixel data.
    scoped_refptr<SerializedScriptValue> input =
        SerializedValue({0xff, 0x12, 0xff, 0x0d, 0x5c, 0x67, 0x01, 0x03, 0x02,
                         0x03, 0x04, 0x01, 0x05, 0x01, 0x00, 0x02, 0x01, 0x11,
                         0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24, 0x00, 0x3c, 0x94,
                         0x3a, 0x3f, 0x28, 0x5f, 0x24, 0x00, 0x3c, 0x00, 0x00});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Too few bytes declared in pixel data.
    scoped_refptr<SerializedScriptValue> input = SerializedValue({
        0xff, 0x12, 0xff, 0x0d, 0x5c, 0x67, 0x01, 0x03, 0x02, 0x03, 0x04,
        0x01, 0x05, 0x01, 0x00, 0x02, 0x01, 0x0f, 0x94, 0x3a, 0x3f, 0x28,
        0x5f, 0x24, 0x00, 0x3c, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24,
    });
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Nonsense for color space data.
    scoped_refptr<SerializedScriptValue> input = SerializedValue(
        {0xff, 0x12, 0xff, 0x0d, 0x5c, 0x67, 0x01, 0x05, 0x02, 0x03, 0x04, 0x01,
         0x05, 0x01, 0x00, 0x02, 0x01, 0x10, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24,
         0x00, 0x3c, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24, 0x00, 0x3c});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Nonsense for pixel format data.
    scoped_refptr<SerializedScriptValue> input = SerializedValue(
        {0xff, 0x12, 0xff, 0x0d, 0x5c, 0x67, 0x01, 0x03, 0x02, 0x04, 0x04, 0x01,
         0x05, 0x01, 0x00, 0x02, 0x01, 0x10, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24,
         0x00, 0x3c, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24, 0x00, 0x3c});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Nonsense for origin clean data.
    scoped_refptr<SerializedScriptValue> input = SerializedValue(
        {0xff, 0x12, 0xff, 0x0d, 0x5c, 0x67, 0x01, 0x03, 0x02, 0x03, 0x04, 0x02,
         0x05, 0x01, 0x00, 0x02, 0x01, 0x10, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24,
         0x00, 0x3c, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24, 0x00, 0x3c});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Nonsense for premultiplied bit.
    scoped_refptr<SerializedScriptValue> input = SerializedValue(
        {0xff, 0x12, 0xff, 0x0d, 0x5c, 0x67, 0x01, 0x03, 0x02, 0x03, 0x04, 0x01,
         0x05, 0x02, 0x00, 0x02, 0x01, 0x10, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24,
         0x00, 0x3c, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24, 0x00, 0x3c});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Wrong size declared in pixel data.
    scoped_refptr<SerializedScriptValue> input = SerializedValue(
        {0xff, 0x12, 0xff, 0x0d, 0x5c, 0x67, 0x01, 0x03, 0x02, 0x03, 0x04, 0x01,
         0x05, 0x01, 0x00, 0x03, 0x01, 0x10, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24,
         0x00, 0x3c, 0x94, 0x3a, 0x3f, 0x28, 0x5f, 0x24, 0x00, 0x3c});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Nonsense image serialization tag (kImageDataStorageFormatTag).
    scoped_refptr<SerializedScriptValue> input =
        SerializedValue({0xff, 0x12, 0xff, 0x0d, 0x5c, 0x67, 0x03, 0x00, 0x00,
                         0x01, 0x01, 0x04, 0x00, 0x00, 0x00, 0x00});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
}

TEST(V8ScriptValueSerializerTest, TransferImageBitmap) {
  test::TaskEnvironment task_environment;
  // More thorough tests exist in web_tests/.
  V8TestingScope scope;

  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(10, 7));
  surface->getCanvas()->clear(SK_ColorRED);
  sk_sp<SkImage> image = surface->makeImageSnapshot();
  auto* image_bitmap = MakeGarbageCollected<ImageBitmap>(
      UnacceleratedStaticBitmapImage::Create(image));
  ASSERT_TRUE(image_bitmap->BitmapImage());

  v8::Local<v8::Value> wrapper =
      ToV8Traits<ImageBitmap>::ToV8(scope.GetScriptState(), image_bitmap);
  Transferables transferables;
  transferables.image_bitmaps.push_back(image_bitmap);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState(), &transferables);
  ImageBitmap* new_image_bitmap =
      V8ImageBitmap::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_image_bitmap, nullptr);
  ASSERT_TRUE(new_image_bitmap->BitmapImage());
  ASSERT_EQ(gfx::Size(10, 7), new_image_bitmap->Size());

  // Check that the pixel at (3, 3) is red.
  uint8_t pixel[4] = {};
  sk_sp<SkImage> new_image = new_image_bitmap->BitmapImage()
                                 ->PaintImageForCurrentFrame()
                                 .GetSwSkImage();
  ASSERT_TRUE(new_image->readPixels(
      SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      &pixel, 4, 3, 3));
  ASSERT_THAT(pixel, testing::ElementsAre(255, 0, 0, 255));

  // Check also that the underlying image contents were transferred.
  EXPECT_EQ(image, new_image);
  EXPECT_TRUE(image_bitmap->IsNeutered());
}

TEST(V8ScriptValueSerializerTest, TransferOffscreenCanvas) {
  test::TaskEnvironment task_environment;
  // More exhaustive tests in web_tests/. This is a sanity check.
  V8TestingScope scope;
  OffscreenCanvas* canvas =
      OffscreenCanvas::Create(scope.GetScriptState(), 10, 7);
  canvas->SetPlaceholderCanvasId(519);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<OffscreenCanvas>::ToV8(scope.GetScriptState(), canvas);
  Transferables transferables;
  transferables.offscreen_canvases.push_back(canvas);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState(), &transferables);
  OffscreenCanvas* new_canvas =
      V8OffscreenCanvas::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_canvas, nullptr);
  EXPECT_EQ(gfx::Size(10, 7), new_canvas->Size());
  EXPECT_EQ(519, new_canvas->PlaceholderCanvasId());
  EXPECT_TRUE(canvas->IsNeutered());
  EXPECT_FALSE(new_canvas->IsNeutered());
}

TEST(V8ScriptValueSerializerTest, RoundTripBlob) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const char kHelloWorld[] = "Hello world!";
  Blob* blob = Blob::Create(
      base::as_bytes(base::span_with_nul_from_cstring(kHelloWorld)),
      "text/plain");
  String uuid = blob->Uuid();
  EXPECT_FALSE(uuid.empty());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<Blob>::ToV8(scope.GetScriptState(), blob);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  Blob* new_blob = V8Blob::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_blob, nullptr);
  EXPECT_EQ("text/plain", new_blob->type());
  EXPECT_EQ(sizeof(kHelloWorld), new_blob->size());
  EXPECT_EQ(uuid, new_blob->Uuid());
}

TEST(V8ScriptValueSerializerTest, DecodeBlob) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x62, 0x24, 0x64, 0x38, 0x37, 0x35, 0x64,
       0x66, 0x63, 0x32, 0x2d, 0x34, 0x35, 0x30, 0x35, 0x2d, 0x34, 0x36,
       0x31, 0x62, 0x2d, 0x39, 0x38, 0x66, 0x65, 0x2d, 0x30, 0x63, 0x66,
       0x36, 0x63, 0x63, 0x35, 0x65, 0x61, 0x66, 0x34, 0x34, 0x0a, 0x74,
       0x65, 0x78, 0x74, 0x2f, 0x70, 0x6c, 0x61, 0x69, 0x6e, 0x0c});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  Blob* new_blob = V8Blob::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_blob, nullptr);
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", new_blob->Uuid());
  EXPECT_EQ("text/plain", new_blob->type());
  EXPECT_EQ(12u, new_blob->size());
}

TEST(V8ScriptValueSerializerTest, RoundTripBlobIndex) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const char kHelloWorld[] = "Hello world!";
  Blob* blob = Blob::Create(
      base::as_bytes(base::span_with_nul_from_cstring(kHelloWorld)),
      "text/plain");
  String uuid = blob->Uuid();
  EXPECT_FALSE(uuid.empty());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<Blob>::ToV8(scope.GetScriptState(), blob);
  WebBlobInfoArray blob_info_array;
  v8::Local<v8::Value> result = RoundTrip(
      wrapper, scope, scope.GetExceptionState(), nullptr, &blob_info_array);

  // As before, the resulting blob should be correct.
  Blob* new_blob = V8Blob::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_blob, nullptr);
  EXPECT_EQ("text/plain", new_blob->type());
  EXPECT_EQ(sizeof(kHelloWorld), new_blob->size());
  EXPECT_EQ(uuid, new_blob->Uuid());

  // The blob info array should also contain the blob details since it was
  // serialized by index into this array.
  ASSERT_EQ(1u, blob_info_array.size());
  const WebBlobInfo& info = blob_info_array[0];
  EXPECT_FALSE(info.IsFile());
  EXPECT_EQ(uuid, String(info.Uuid()));
  EXPECT_EQ("text/plain", info.GetType());
  EXPECT_EQ(sizeof(kHelloWorld), static_cast<size_t>(info.size()));
}

TEST(V8ScriptValueSerializerTest, DecodeBlobIndex) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x69, 0x00});
  WebBlobInfoArray blob_info_array;
  blob_info_array.emplace_back(WebBlobInfo::BlobForTesting(
      "d875dfc2-4505-461b-98fe-0cf6cc5eaf44", "text/plain", 12));
  V8ScriptValueDeserializer::Options options;
  options.blob_info = &blob_info_array;
  V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                         options);
  v8::Local<v8::Value> result = deserializer.Deserialize();
  Blob* new_blob = V8Blob::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_blob, nullptr);
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", new_blob->Uuid());
  EXPECT_EQ("text/plain", new_blob->type());
  EXPECT_EQ(12u, new_blob->size());
}

TEST(V8ScriptValueSerializerTest, DecodeBlobIndexOutOfRange) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x69, 0x01});
  {
    V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
  {
    WebBlobInfoArray blob_info_array;
    blob_info_array.emplace_back(WebBlobInfo::BlobForTesting(
        "d875dfc2-4505-461b-98fe-0cf6cc5eaf44", "text/plain", 12));
    V8ScriptValueDeserializer::Options options;
    options.blob_info = &blob_info_array;
    V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                           options);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
}

TEST(V8ScriptValueSerializerTest, RoundTripFileNative) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FileBackedBlobFactoryTestHelper file_factory_helper(
      scope.GetExecutionContext());
  auto* file =
      MakeGarbageCollected<File>(scope.GetExecutionContext(), "/native/path");
  file_factory_helper.FlushForTesting();
  v8::Local<v8::Value> wrapper =
      ToV8Traits<File>::ToV8(scope.GetScriptState(), file);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_TRUE(new_file->HasBackingFile());
  EXPECT_EQ("/native/path", new_file->GetPath());
  EXPECT_TRUE(new_file->FileSystemURL().IsEmpty());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileBackedByBlob) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  const base::Time kModificationTime = base::Time::UnixEpoch();
  scoped_refptr<BlobDataHandle> blob_data_handle = BlobDataHandle::Create();
  auto* file = MakeGarbageCollected<File>("/native/path", kModificationTime,
                                          blob_data_handle);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<File>::ToV8(scope.GetScriptState(), file);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_FALSE(new_file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().empty());
  EXPECT_TRUE(new_file->FileSystemURL().IsEmpty());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileNativeSnapshot) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FileMetadata metadata;
  metadata.platform_path = "/native/snapshot";
  auto* context = scope.GetExecutionContext();
  FileBackedBlobFactoryTestHelper helper(context);
  File* file = File::CreateForFileSystemFile(context, "name", metadata,
                                             File::kIsUserVisible);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<File>::ToV8(scope.GetScriptState(), file);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_TRUE(new_file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", new_file->GetPath());
  EXPECT_TRUE(new_file->FileSystemURL().IsEmpty());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileNonNativeSnapshot) {
  test::TaskEnvironment task_environment;
  // Preserving behavior, filesystem URL is not preserved across cloning.
  KURL url("filesystem:http://example.com/isolated/hash/non-native-file");
  V8TestingScope scope;
  FileMetadata metadata;
  metadata.length = 0;
  File* file = File::CreateForFileSystemFile(
      url, metadata, File::kIsUserVisible, BlobDataHandle::Create());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<File>::ToV8(scope.GetScriptState(), file);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_FALSE(new_file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().empty());
  EXPECT_TRUE(new_file->FileSystemURL().IsEmpty());
}

// Used for checking that times provided are between now and the current time
// when the checker was constructed, according to base::Time::Now.
class TimeIntervalChecker {
 public:
  TimeIntervalChecker() : start_time_(NowInMilliseconds()) {}

  bool WasAliveAt(int64_t time_in_milliseconds) {
    return start_time_ <= time_in_milliseconds &&
           time_in_milliseconds <= NowInMilliseconds();
  }

 private:
  static int64_t NowInMilliseconds() {
    return (base::Time::Now() - base::Time::UnixEpoch()).InMilliseconds();
  }

  const int64_t start_time_;
};

TEST(V8ScriptValueSerializerTest, DecodeFileV3) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  TimeIntervalChecker time_interval_checker;
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x03, 0x3f, 0x00, 0x66, 0x04, 'p', 'a', 't', 'h', 0x24, 'f',
       '4',  'a',  '6',  'e',  'd',  'd',  '5', '-', '6', '5', 'a',  'd',
       '-',  '4',  'd',  'c',  '3',  '-',  'b', '6', '7', 'c', '-',  'a',
       '7',  '7',  '9',  'c',  '0',  '2',  'f', '0', 'f', 'a', '3',  0x0a,
       't',  'e',  'x',  't',  '/',  'p',  'l', 'a', 'i', 'n'});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_FALSE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(0u, new_file->size());
  EXPECT_TRUE(time_interval_checker.WasAliveAt(new_file->lastModified()));
  EXPECT_EQ(File::kIsUserVisible, new_file->GetUserVisibility());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV4) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  TimeIntervalChecker time_interval_checker;
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x04, 0x3f, 0x00, 0x66, 0x04, 'p', 'a',  't',  'h', 0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4', 'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-', '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7', '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't', 'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n', 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("rel", new_file->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_FALSE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(0u, new_file->size());
  EXPECT_TRUE(time_interval_checker.WasAliveAt(new_file->lastModified()));
  EXPECT_EQ(File::kIsUserVisible, new_file->GetUserVisibility());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV4WithSnapshot) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x04, 0x3f, 0x00, 0x66, 0x04, 'p', 'a',  't',  'h',  0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4',  'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-',  '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7',  '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't',  'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n', 0x01, 0x80, 0x04, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0xd0, 0xbf});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("rel", new_file->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_TRUE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(512u, new_file->size());
  // From v4 to v7, the last modified time is written in seconds.
  // So -0.25 represents 250 ms before the Unix epoch.
  EXPECT_EQ(-250, new_file->lastModified());
  EXPECT_EQ(base::Milliseconds(-250.0),
            new_file->LastModifiedTime() - base::Time::UnixEpoch());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV7) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  TimeIntervalChecker time_interval_checker;
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x07, 0x3f, 0x00, 0x66, 0x04, 'p', 'a',  't',  'h', 0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4', 'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-', '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7', '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't', 'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n', 0x00, 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("rel", new_file->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_FALSE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(0u, new_file->size());
  EXPECT_TRUE(time_interval_checker.WasAliveAt(new_file->lastModified()));
  EXPECT_EQ(File::kIsNotUserVisible, new_file->GetUserVisibility());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV8WithSnapshot) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x08, 0x3f, 0x00, 0x66, 0x04, 'p',  'a',  't',  'h',  0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l',  0x24, 'f',  '4',  'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5',  'a',  'd',  '-',  '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c',  '-',  'a',  '7',  '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a',  '3',  0x0a, 't',  'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n',  0x01, 0x80, 0x04, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0xd0, 0xbf, 0x01, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("rel", new_file->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_TRUE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(512u, new_file->size());
  // From v8, the last modified time is written in milliseconds.
  // So -0.25 represents 0.25 ms before the Unix epoch.
  EXPECT_EQ(base::Milliseconds(-0.25),
            new_file->LastModifiedTime() - base::Time::UnixEpoch());
  // lastModified IDL attribute can't represent -0.25 ms.
  EXPECT_EQ(INT64_C(0), new_file->lastModified());
  EXPECT_EQ(File::kIsUserVisible, new_file->GetUserVisibility());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileIndex) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FileBackedBlobFactoryTestHelper file_factory_helper(
      scope.GetExecutionContext());
  auto* file =
      MakeGarbageCollected<File>(scope.GetExecutionContext(), "/native/path");
  file_factory_helper.FlushForTesting();
  v8::Local<v8::Value> wrapper =
      ToV8Traits<File>::ToV8(scope.GetScriptState(), file);
  WebBlobInfoArray blob_info_array;
  v8::Local<v8::Value> result = RoundTrip(
      wrapper, scope, scope.GetExceptionState(), nullptr, &blob_info_array);

  // As above, the resulting blob should be correct.
  // The only users of the 'blob_info_array' version of serialization is
  // IndexedDB, and the full path is not needed for that system - thus it is not
  // sent in the round trip.
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_FALSE(new_file->HasBackingFile());
  EXPECT_EQ("path", new_file->name());
  EXPECT_TRUE(new_file->FileSystemURL().IsEmpty());

  // The blob info array should also contain the details since it was serialized
  // by index into this array.
  ASSERT_EQ(1u, blob_info_array.size());
  const WebBlobInfo& info = blob_info_array[0];
  EXPECT_TRUE(info.IsFile());
  EXPECT_EQ("path", info.FileName());
  EXPECT_EQ(file->Uuid(), String(info.Uuid()));
}

TEST(V8ScriptValueSerializerTest, DecodeFileIndex) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x65, 0x00});
  WebBlobInfoArray blob_info_array;
  blob_info_array.emplace_back(WebBlobInfo::FileForTesting(
      "d875dfc2-4505-461b-98fe-0cf6cc5eaf44", "path", "text/plain"));
  V8ScriptValueDeserializer::Options options;
  options.blob_info = &blob_info_array;
  V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                         options);
  v8::Local<v8::Value> result = deserializer.Deserialize();
  File* new_file = V8File::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file, nullptr);
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_TRUE(new_file->GetPath().empty());
  EXPECT_EQ("path", new_file->name());
}

TEST(V8ScriptValueSerializerTest, DecodeFileIndexOutOfRange) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x65, 0x01});
  {
    V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
  {
    WebBlobInfoArray blob_info_array;
    blob_info_array.emplace_back(WebBlobInfo::FileForTesting(
        "d875dfc2-4505-461b-98fe-0cf6cc5eaf44", "path", "text/plain"));
    V8ScriptValueDeserializer::Options options;
    options.blob_info = &blob_info_array;
    V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                           options);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
}

// Most of the logic for FileList is shared with File, so the tests here are
// fairly basic.

TEST(V8ScriptValueSerializerTest, RoundTripFileList) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FileBackedBlobFactoryTestHelper file_factory_helper(
      scope.GetExecutionContext());
  auto* file_list = MakeGarbageCollected<FileList>();
  file_list->Append(
      MakeGarbageCollected<File>(scope.GetExecutionContext(), "/native/path"));
  file_list->Append(
      MakeGarbageCollected<File>(scope.GetExecutionContext(), "/native/path2"));
  file_factory_helper.FlushForTesting();
  v8::Local<v8::Value> wrapper =
      ToV8Traits<FileList>::ToV8(scope.GetScriptState(), file_list);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  FileList* new_file_list = V8FileList::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file_list, nullptr);
  ASSERT_EQ(2u, new_file_list->length());
  EXPECT_EQ("/native/path", new_file_list->item(0)->GetPath());
  EXPECT_EQ("/native/path2", new_file_list->item(1)->GetPath());
}

TEST(V8ScriptValueSerializerTest, DecodeEmptyFileList) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x6c, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  FileList* new_file_list = V8FileList::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file_list, nullptr);
  EXPECT_EQ(0u, new_file_list->length());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListWithInvalidLength) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x6c, 0x01});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  EXPECT_TRUE(result->IsNull());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListV8WithoutSnapshot) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  TimeIntervalChecker time_interval_checker;
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x08, 0x3f, 0x00, 0x6c, 0x01, 0x04, 'p', 'a',  't',  'h', 0x04,
       'n',  'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4', 'a',
       '6',  'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-', '4',
       'd',  'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7', '7',
       '9',  'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't', 'e',
       'x',  't',  '/',  'p',  'l',  'a',  'i',  'n', 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  FileList* new_file_list = V8FileList::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file_list, nullptr);
  EXPECT_EQ(1u, new_file_list->length());
  File* new_file = new_file_list->item(0);
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("rel", new_file->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_FALSE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(0u, new_file->size());
  EXPECT_TRUE(time_interval_checker.WasAliveAt(new_file->lastModified()));
  EXPECT_EQ(File::kIsNotUserVisible, new_file->GetUserVisibility());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileListIndex) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  FileBackedBlobFactoryTestHelper file_factory_helper(
      scope.GetExecutionContext());
  auto* file_list = MakeGarbageCollected<FileList>();
  file_list->Append(
      MakeGarbageCollected<File>(scope.GetExecutionContext(), "/native/path"));
  file_list->Append(
      MakeGarbageCollected<File>(scope.GetExecutionContext(), "/native/path2"));
  file_factory_helper.FlushForTesting();
  v8::Local<v8::Value> wrapper =
      ToV8Traits<FileList>::ToV8(scope.GetScriptState(), file_list);
  WebBlobInfoArray blob_info_array;
  v8::Local<v8::Value> result = RoundTrip(
      wrapper, scope, scope.GetExceptionState(), nullptr, &blob_info_array);

  // FileList should be produced correctly.
  // The only users of the 'blob_info_array' version of serialization is
  // IndexedDB, and the full path is not needed for that system - thus it is not
  // sent in the round trip.
  FileList* new_file_list = V8FileList::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file_list, nullptr);
  ASSERT_EQ(2u, new_file_list->length());
  EXPECT_EQ("path", new_file_list->item(0)->name());
  EXPECT_EQ("path2", new_file_list->item(1)->name());

  // And the blob info array should be populated.
  ASSERT_EQ(2u, blob_info_array.size());
  EXPECT_TRUE(blob_info_array[0].IsFile());
  EXPECT_EQ("path", blob_info_array[0].FileName());
  EXPECT_TRUE(blob_info_array[1].IsFile());
  EXPECT_EQ("path2", blob_info_array[1].FileName());
}

TEST(V8ScriptValueSerializerTest, DecodeEmptyFileListIndex) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4c, 0x00});
  WebBlobInfoArray blob_info_array;
  V8ScriptValueDeserializer::Options options;
  options.blob_info = &blob_info_array;
  V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                         options);
  v8::Local<v8::Value> result = deserializer.Deserialize();
  FileList* new_file_list = V8FileList::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_file_list, nullptr);
  EXPECT_EQ(0u, new_file_list->length());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListIndexWithInvalidLength) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4c, 0x02});
  WebBlobInfoArray blob_info_array;
  V8ScriptValueDeserializer::Options options;
  options.blob_info = &blob_info_array;
  V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                         options);
  v8::Local<v8::Value> result = deserializer.Deserialize();
  EXPECT_TRUE(result->IsNull());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListIndex) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4c, 0x01, 0x00, 0x00});
  WebBlobInfoArray blob_info_array;
  blob_info_array.emplace_back(WebBlobInfo::FileForTesting(
      "d875dfc2-4505-461b-98fe-0cf6cc5eaf44", "name", "text/plain"));
  V8ScriptValueDeserializer::Options options;
  options.blob_info = &blob_info_array;
  V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                         options);
  v8::Local<v8::Value> result = deserializer.Deserialize();
  FileList* new_file_list = V8FileList::ToWrappable(scope.GetIsolate(), result);
  EXPECT_EQ(1u, new_file_list->length());
  File* new_file = new_file_list->item(0);
  EXPECT_TRUE(new_file->GetPath().empty());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
}

TEST(V8ScriptValueSerializerTest, DecodeHardcodedNullValue) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  EXPECT_TRUE(V8ScriptValueDeserializer(scope.GetScriptState(),
                                        SerializedScriptValue::NullValue())
                  .Deserialize()
                  ->IsNull());
}

// This is not the most efficient way to write a small version, but it's
// technically admissible. We should handle this in a consistent way to avoid
// DCHECK failure. Thus this is "true" encoded slightly strangely.
TEST(V8ScriptValueSerializerTest, DecodeWithInefficientVersionEnvelope) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x90, 0x00, 0xff, 0x09, 0x54});
  EXPECT_TRUE(
      V8ScriptValueDeserializer(scope.GetScriptState(), std::move(input))
          .Deserialize()
          ->IsTrue());
}

// Sanity check for transferring ReadableStreams. This is mostly tested via
// web tests.
TEST(V8ScriptValueSerializerTest, RoundTripReadableStream) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto* script_state = scope.GetScriptState();

  auto* rs = ReadableStream::Create(script_state, ASSERT_NO_EXCEPTION);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<ReadableStream>::ToV8(scope.GetScriptState(), rs);
  HeapVector<ScriptValue> transferable_array = {ScriptValue(isolate, wrapper)};
  Transferables transferables;
  ASSERT_TRUE(SerializedScriptValue::ExtractTransferables(
      isolate, transferable_array, transferables, ASSERT_NO_EXCEPTION));
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, ASSERT_NO_EXCEPTION, &transferables);
  EXPECT_TRUE(result->IsObject());
  ReadableStream* transferred = V8ReadableStream::ToWrappable(isolate, result);
  ASSERT_NE(transferred, nullptr);
  EXPECT_NE(rs, transferred);
  EXPECT_TRUE(rs->locked());
  EXPECT_FALSE(transferred->locked());
}

TEST(V8ScriptValueSerializerTest, TransformStreamIntegerOverflow) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  auto* isolate = scope.GetIsolate();
  auto* script_state = scope.GetScriptState();

  // Create a real SerializedScriptValue so that the MessagePorts are set up
  // properly.
  auto* ts = TransformStream::Create(script_state, ASSERT_NO_EXCEPTION);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<TransformStream>::ToV8(scope.GetScriptState(), ts);
  HeapVector<ScriptValue> transferable_array = {ScriptValue(isolate, wrapper)};
  Transferables transferables;
  ASSERT_TRUE(SerializedScriptValue::ExtractTransferables(
      isolate, transferable_array, transferables, ASSERT_NO_EXCEPTION));

  // Extract message ports and disentangle them.
  Vector<MessagePortChannel> channels = MessagePort::DisentanglePorts(
      scope.GetExecutionContext(), transferables.message_ports,
      ASSERT_NO_EXCEPTION);

  V8ScriptValueSerializer::Options serialize_options;
  serialize_options.transferables = &transferables;
  V8ScriptValueSerializer serializer(script_state, serialize_options);
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      serializer.Serialize(wrapper, ASSERT_NO_EXCEPTION);
  ASSERT_TRUE(serialized_script_value);

  // Now create a corrupted SerializedScriptValue using the same message ports.
  // The final 5 bytes is the offset of the two message ports inside the
  // transferred message port array. In order to trigger integer overflow this
  // is set to 0xffffffff, encoded as a varint.
  uint8_t serialized_value[] = {0xff, 0x14, 0xff, 0x0d, 0x5c, 0x6d,
                                0xff, 0xff, 0xff, 0xff, 0x0f};

  auto corrupted_serialized_script_value =
      SerializedScriptValue::Create(serialized_value);
  corrupted_serialized_script_value->GetStreams() =
      std::move(serialized_script_value->GetStreams());

  // Entangle the message ports.
  MessagePortArray* transferred_message_ports = MessagePort::EntanglePorts(
      *scope.GetExecutionContext(), std::move(channels));

  UnpackedSerializedScriptValue* unpacked = SerializedScriptValue::Unpack(
      std::move(corrupted_serialized_script_value));
  V8ScriptValueDeserializer::Options deserialize_options;
  deserialize_options.message_ports = transferred_message_ports;
  V8ScriptValueDeserializer deserializer(script_state, unpacked,
                                         deserialize_options);
  // If this doesn't crash then the test succeeded.
  v8::Local<v8::Value> result = deserializer.Deserialize();

  // Deserialization should have failed.
  EXPECT_TRUE(result->IsNull());
}

TEST(V8ScriptValueSerializerTest, RoundTripDOMException) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  DOMException* exception =
      DOMException::Create("message", "InvalidStateError");
  v8::Local<v8::Value> wrapper =
      ToV8Traits<DOMException>::ToV8(scope.GetScriptState(), exception);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  DOMException* new_exception =
      V8DOMException::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_exception, nullptr);
  EXPECT_NE(exception, new_exception);
  EXPECT_EQ(exception->code(), new_exception->code());
  EXPECT_EQ(exception->name(), new_exception->name());
  EXPECT_EQ(exception->message(), new_exception->message());
}

TEST(V8ScriptValueSerializerTest, DecodeDOMExceptionWithInvalidNameString) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x13, 0xff, 0x0d, 0x5c, 0x78, 0x01, 0xff, 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  EXPECT_TRUE(result->IsNull());
}

TEST(V8ScriptValueSerializerTest, NoSharedValueConveyor) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x14, 0xff, 0x0f, 'p', 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  EXPECT_TRUE(result->IsNull());
}

TEST(V8ScriptValueSerializerTest, CanDeserializeIn_OldValues) {
  test::TaskEnvironment task_environment;
  // This is `true` serialized in version 9. It should still return true from
  // CanDeserializeIn.
  V8TestingScope scope;
  scoped_refptr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 'T', 0x00});
  EXPECT_TRUE(input->CanDeserializeIn(scope.GetExecutionContext()));
}

TEST(V8ScriptValueSerializerTest, RoundTripFencedFrameConfig) {
  test::TaskEnvironment task_environment;
  ScopedFencedFramesForTest fenced_frames(true);
  V8TestingScope scope;
  FencedFrameConfig* config = FencedFrameConfig::Create(
      KURL("https://example.com"), "some shared storage context",
      KURL("urn:uuid:37665e6f-f3fd-4393-8429-719d02843a54"), gfx::Size(64, 48),
      gfx::Size(32, 16), FencedFrameConfig::AttributeVisibility::kOpaque, true);
  v8::Local<v8::Value> wrapper =
      ToV8Traits<FencedFrameConfig>::ToV8(scope.GetScriptState(), config);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  FencedFrameConfig* new_config =
      V8FencedFrameConfig::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_config, nullptr);
  EXPECT_NE(config, new_config);
  EXPECT_EQ(config->url_, new_config->url_);
  EXPECT_EQ(config->shared_storage_context_,
            new_config->shared_storage_context_);
  EXPECT_EQ(config->urn_uuid_, new_config->urn_uuid_);
  EXPECT_EQ(config->container_size_, new_config->container_size_);
  EXPECT_EQ(config->content_size_, new_config->content_size_);
  EXPECT_EQ(config->url_attribute_visibility_,
            new_config->url_attribute_visibility_);
  EXPECT_EQ(config->deprecated_should_freeze_initial_size_,
            new_config->deprecated_should_freeze_initial_size_);
}

TEST(V8ScriptValueSerializerTest, RoundTripFencedFrameConfigNullValues) {
  test::TaskEnvironment task_environment;
  ScopedFencedFramesForTest fenced_frames(true);
  V8TestingScope scope;
  FencedFrameConfig* config = FencedFrameConfig::Create(g_empty_string);
  ASSERT_FALSE(config->urn_uuid_.has_value());
  ASSERT_FALSE(config->container_size_.has_value());
  ASSERT_FALSE(config->content_size_.has_value());
  v8::Local<v8::Value> wrapper =
      ToV8Traits<FencedFrameConfig>::ToV8(scope.GetScriptState(), config);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, scope.GetExceptionState());
  FencedFrameConfig* new_config =
      V8FencedFrameConfig::ToWrappable(scope.GetIsolate(), result);
  ASSERT_NE(new_config, nullptr);
  EXPECT_NE(config, new_config);
  EXPECT_EQ(config->shared_storage_context_,
            new_config->shared_storage_context_);
  EXPECT_EQ(config->urn_uuid_, new_config->urn_uuid_);
  EXPECT_FALSE(new_config->urn_uuid_.has_value());
  EXPECT_EQ(config->container_size_, new_config->container_size_);
  EXPECT_FALSE(new_config->container_size_.has_value());
  EXPECT_EQ(config->content_size_, new_config->content_size_);
  EXPECT_FALSE(new_config->content_size_.has_value());
}

namespace {

class GinWrappable : public gin::Wrappable<GinWrappable> {
 public:
  static v8::Local<v8::Object> Create(v8::Isolate* isolate) {
    auto* instance = new GinWrappable();
    return instance->GetWrapper(isolate).ToLocalChecked();
  }
  ~GinWrappable() override = default;

  static gin::WrapperInfo kWrapperInfo;

 private:
  GinWrappable() = default;
};

gin::WrapperInfo GinWrappable::kWrapperInfo = {gin::kEmbedderNativeGin};

}  // namespace

TEST(V8ScriptValueSerializerTest, CoexistWithGin) {
  test::TaskEnvironment task_environment;
  V8TestingScope scope;
  v8::Isolate* const isolate = scope.GetIsolate();
  v8::Local<v8::Object> wrapper = GinWrappable::Create(isolate);
  v8::TryCatch try_catch(scope.GetIsolate());
  scoped_refptr<SerializedScriptValue> serialized_script_value =
      V8ScriptValueSerializer(scope.GetScriptState())
          .Serialize(wrapper, PassThroughException(scope.GetIsolate()));
  // Serializing a gin value will throw an exception, which is fine.
  // We just want to make sure it does not crash.
  EXPECT_TRUE(try_catch.HasCaught());
  EXPECT_FALSE(serialized_script_value);
}

}  // namespace blink
