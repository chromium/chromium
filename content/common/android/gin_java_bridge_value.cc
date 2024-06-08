// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "content/common/android/gin_java_bridge_value.h"

#include "base/containers/span.h"
#include "base/pickle.h"

namespace content {

namespace {

// The magic value is only used to prevent accidental attempts of reading
// GinJavaBridgeValue from a random BinaryValue.  GinJavaBridgeValue is not
// intended for scenarios where with BinaryValues are being used for anything
// else than holding GinJavaBridgeValues.  If a need for such scenario ever
// emerges, the best solution would be to extend GinJavaBridgeValue to be able
// to wrap raw BinaryValues.
const uint32_t kHeaderMagic = 0xBEEFCAFE;

#pragma pack(push, 4)
struct Header : public base::Pickle::Header {
  uint32_t magic;
  int32_t type;
};
#pragma pack(pop)

}

// static
std::unique_ptr<base::Value> GinJavaBridgeValue::CreateUndefinedValue() {
  GinJavaBridgeValue gin_value(TYPE_UNDEFINED);
  return gin_value.SerializeToBinaryValue();
}

// static
std::unique_ptr<base::Value> GinJavaBridgeValue::CreateNonFiniteValue(
    float in_value) {
  GinJavaBridgeValue gin_value(TYPE_NONFINITE);
  gin_value.pickle_.WriteFloat(in_value);
  return gin_value.SerializeToBinaryValue();
}

// static
std::unique_ptr<base::Value> GinJavaBridgeValue::CreateNonFiniteValue(
    double in_value) {
  return CreateNonFiniteValue(static_cast<float>(in_value));
}

// static
std::unique_ptr<base::Value> GinJavaBridgeValue::CreateObjectIDValue(
    int32_t in_value) {
  GinJavaBridgeValue gin_value(TYPE_OBJECT_ID);
  gin_value.pickle_.WriteInt(in_value);
  return gin_value.SerializeToBinaryValue();
}

// static
std::unique_ptr<base::Value> GinJavaBridgeValue::CreateUInt32Value(
    uint32_t in_value) {
  GinJavaBridgeValue gin_value(TYPE_UINT32);
  gin_value.pickle_.WriteUInt32(in_value);
  return gin_value.SerializeToBinaryValue();
}

// static
bool GinJavaBridgeValue::ContainsGinJavaBridgeValue(const base::Value* value) {
  if (!value->is_blob())
    return false;
  if (value->GetBlob().size() < sizeof(Header))
    return false;
  base::Pickle pickle = base::Pickle::WithUnownedBuffer(value->GetBlob());
  // Broken binary value: payload or header size is wrong
  if (!pickle.data() || pickle.size() - pickle.payload_size() != sizeof(Header))
    return false;
  Header* header = pickle.headerT<Header>();
  return (header->magic == kHeaderMagic &&
          header->type >= TYPE_FIRST_VALUE && header->type < TYPE_LAST_VALUE);
}

// static
std::unique_ptr<const GinJavaBridgeValue> GinJavaBridgeValue::FromValue(
    const base::Value* value) {
  return std::unique_ptr<const GinJavaBridgeValue>(
      value->is_blob() ? new GinJavaBridgeValue(value) : NULL);
}

GinJavaBridgeValue::Type GinJavaBridgeValue::GetType() const {
  const Header* header = pickle_.headerT<Header>();
  DCHECK(header->type >= TYPE_FIRST_VALUE && header->type < TYPE_LAST_VALUE);
  return static_cast<Type>(header->type);
}

bool GinJavaBridgeValue::IsType(Type type) const {
  return GetType() == type;
}

bool GinJavaBridgeValue::GetAsNonFinite(float* out_value) const {
  if (GetType() == TYPE_NONFINITE) {
    base::PickleIterator iter(pickle_);
    return iter.ReadFloat(out_value);
  } else {
    return false;
  }
}

bool GinJavaBridgeValue::GetAsObjectID(int32_t* out_object_id) const {
  if (GetType() == TYPE_OBJECT_ID) {
    base::PickleIterator iter(pickle_);
    return iter.ReadInt(out_object_id);
  } else {
    return false;
  }
}

bool GinJavaBridgeValue::GetAsUInt32(uint32_t* out_value) const {
  if (GetType() == TYPE_UINT32) {
    base::PickleIterator iter(pickle_);
    return iter.ReadUInt32(out_value);
  } else {
    return false;
  }
}

GinJavaBridgeValue::GinJavaBridgeValue(Type type) :
    pickle_(sizeof(Header)) {
  Header* header = pickle_.headerT<Header>();
  header->magic = kHeaderMagic;
  header->type = type;
}

GinJavaBridgeValue::GinJavaBridgeValue(const base::Value* value)
    : pickle_(base::Pickle::WithUnownedBuffer(value->GetBlob())) {
  DCHECK(ContainsGinJavaBridgeValue(value));
}

std::unique_ptr<base::Value> GinJavaBridgeValue::SerializeToBinaryValue() {
  const auto* data = static_cast<const uint8_t*>(pickle_.data());
  return base::Value::ToUniquePtrValue(
      base::Value(base::make_span(data, pickle_.size())));
}

}  // namespace content
