// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gvariant_ref.h"

#include <glib.h>
#include <glibconfig.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"
#include "remoting/host/base/loggable.h"
#include "remoting/host/linux/gvariant_type.h"

namespace remoting::gvariant {

// GVariantRef implementation

GVariantBase::operator GVariantRef<>() const {
  return GVariantRef<>::Ref(raw());
}

GVariant* GVariantBase::raw() const {
  return variant_.get();
}

GVariant* GVariantBase::release() && {
  return variant_.release();
}

Type<> GVariantBase::GetType() const {
  return Type(g_variant_get_type(variant_.get()));
}

GVariantBase::GVariantBase() = default;

GVariantBase::GVariantBase(GVariantPtr variant)
    : variant_(std::move(variant)) {}

GVariantBase::GVariantBase(const GVariantBase& other) = default;

GVariantBase::GVariantBase(GVariantBase&& other) = default;

GVariantBase& GVariantBase::operator=(const GVariantBase& other) = default;

GVariantBase& GVariantBase::operator=(GVariantBase&& other) = default;

GVariantBase::~GVariantBase() = default;

bool operator==(const GVariantBase& lhs, const GVariantBase& rhs) {
  return g_variant_equal(lhs.raw(), rhs.raw());
}

// GVariantRef implementation

// static
template <Type C>
GVariantRef<C> GVariantRef<C>::Take(GVariant* variant)
  requires(C == Type("*"))
{
  return GVariantRef<C>(GVariantPtr::Take(variant));
}
template GVariantRef<Type("*")> GVariantRef<Type("*")>::Take(GVariant* variant);

// static
template <Type C>
GVariantRef<C> GVariantRef<C>::Ref(GVariant* variant)
  requires(C == Type("*"))
{
  return GVariantRef<C>(GVariantPtr::Ref(variant));
}
template GVariantRef<Type("*")> GVariantRef<Type("*")>::Ref(GVariant* variant);

// static
template <Type C>
GVariantRef<C> GVariantRef<C>::RefSink(GVariant* variant)
  requires(C == Type("*"))
{
  return GVariantRef<C>(GVariantPtr::RefSink(variant));
}
template GVariantRef<Type("*")> GVariantRef<Type("*")>::RefSink(
    GVariant* variant);

// Wrapper implementations

ObjectPathCStr::ObjectPathCStr(const ObjectPath& path LIFETIME_BOUND)
    : path_(path.c_str()) {}

// static
base::expected<ObjectPathCStr, Loggable> ObjectPathCStr::TryFrom(
    const char* path LIFETIME_BOUND) {
  if (g_variant_is_object_path(path)) {
    return base::ok(ObjectPathCStr(path, Checked()));
  } else {
    return base::unexpected(
        Loggable(FROM_HERE,
                 base::StrCat({"String is not a valid object path: ", path})));
  }
}

ObjectPathCStr::ObjectPathCStr(const char* path LIFETIME_BOUND, Checked)
    : path_(path) {}

ObjectPath::ObjectPath() : path_("/") {}

ObjectPath::ObjectPath(ObjectPathCStr path) : path_(path.c_str()) {}

// static
base::expected<ObjectPath, Loggable> ObjectPath::TryFrom(std::string path) {
  return ObjectPathCStr::TryFrom(path.c_str()).transform([&](ObjectPathCStr) {
    return ObjectPath(std::move(path));
  });
}

const std::string& ObjectPath::value() const {
  return path_;
}

const char* ObjectPath::c_str() const {
  return path_.c_str();
}

ObjectPath::ObjectPath(std::string path) : path_(path) {}

TypeSignatureCStr::TypeSignatureCStr(
    const TypeSignature& signature LIFETIME_BOUND)
    : signature_(signature.c_str()) {}

// static
base::expected<TypeSignatureCStr, Loggable> TypeSignatureCStr::TryFrom(
    const char* signature LIFETIME_BOUND) {
  if (g_variant_is_signature(signature)) {
    return base::ok(TypeSignatureCStr(signature, Checked()));
  } else {
    return base::unexpected(Loggable(
        FROM_HERE,
        base::StrCat({"String is not a valid type signature: ", signature})));
  }
}

TypeSignatureCStr::TypeSignatureCStr(const char* signature LIFETIME_BOUND,
                                     Checked)
    : signature_(signature) {}

TypeSignature::TypeSignature() = default;

TypeSignature::TypeSignature(TypeSignatureCStr signature)
    : signature_(signature.c_str()) {}

// static
base::expected<TypeSignature, Loggable> TypeSignature::TryFrom(
    std::string signature) {
  return TypeSignatureCStr::TryFrom(signature.c_str())
      .transform([&](TypeSignatureCStr) {
        return TypeSignature(std::move(signature));
      });
}

const std::string& TypeSignature::value() const {
  return signature_;
}

const char* TypeSignature::c_str() const {
  return signature_.c_str();
}

TypeSignature::TypeSignature(std::string path) : signature_(path) {}

// Mapping implementations

// static
auto Mapping<bool>::From(bool value) -> GVariantRef<kType> {
  return GVariantRef<kType>::TakeUnchecked(g_variant_new_boolean(value));
}

// static
bool Mapping<bool>::Into(const GVariantRef<kType>& variant) {
  return g_variant_get_boolean(variant.raw());
}

// static
auto Mapping<std::uint8_t>::From(std::uint8_t value) -> GVariantRef<kType> {
  return GVariantRef<kType>::TakeUnchecked(g_variant_new_byte(value));
}

// static
std::uint8_t Mapping<std::uint8_t>::Into(const GVariantRef<kType>& variant) {
  return g_variant_get_byte(variant.raw());
}

// static
auto Mapping<std::int16_t>::From(std::int16_t value) -> GVariantRef<kType> {
  return GVariantRef<kType>::TakeUnchecked(g_variant_new_int16(value));
}

// static
std::int16_t Mapping<std::int16_t>::Into(const GVariantRef<kType>& variant) {
  return g_variant_get_int16(variant.raw());
}

// static
auto Mapping<std::uint16_t>::From(std::uint16_t value) -> GVariantRef<kType> {
  return GVariantRef<kType>::TakeUnchecked(g_variant_new_uint16(value));
}

// static
std::uint16_t Mapping<std::uint16_t>::Into(const GVariantRef<kType>& variant) {
  return g_variant_get_uint16(variant.raw());
}

// static
auto Mapping<std::int32_t>::From(std::int32_t value) -> GVariantRef<kType> {
  return GVariantRef<kType>::TakeUnchecked(g_variant_new_int32(value));
}

// static
std::int32_t Mapping<std::int32_t>::Into(const GVariantRef<kType>& variant) {
  return g_variant_get_int32(variant.raw());
}

// static
auto Mapping<std::uint32_t>::From(std::uint32_t value) -> GVariantRef<kType> {
  return GVariantRef<kType>::TakeUnchecked(g_variant_new_uint32(value));
}

// static
std::uint32_t Mapping<std::uint32_t>::Into(const GVariantRef<kType>& variant) {
  return g_variant_get_uint32(variant.raw());
}

// static
auto Mapping<std::int64_t>::From(std::int64_t value) -> GVariantRef<kType> {
  return GVariantRef<kType>::TakeUnchecked(g_variant_new_int64(value));
}

// static
std::int64_t Mapping<std::int64_t>::Into(const GVariantRef<kType>& variant) {
  return g_variant_get_int64(variant.raw());
}

// static
auto Mapping<std::uint64_t>::From(std::uint64_t value) -> GVariantRef<kType> {
  return GVariantRef<kType>::TakeUnchecked(g_variant_new_uint64(value));
}

// static
std::uint64_t Mapping<std::uint64_t>::Into(const GVariantRef<kType>& variant) {
  return g_variant_get_uint64(variant.raw());
}

// static
auto Mapping<double>::From(double value) -> GVariantRef<kType> {
  return GVariantRef<kType>::TakeUnchecked(g_variant_new_double(value));
}

// static
double Mapping<double>::Into(const GVariantRef<kType>& variant) {
  return g_variant_get_double(variant.raw());
}

// Creates a GVariantRef of a string-like type ("s", "o", or "g") from a
// string_view that has already been verified to be of the correct form for the
// type.
template <Type C>
  requires(C == Type("s") || C == Type("o") || C == Type("g"))
static GVariantRef<C> CreateStringVariantUnchecked(std::string_view value) {
  // g_variant_new_string() can't be used directly because it requires a null-
  // terminated string, which |value| might not be. To avoid making two copies
  // of |value| (one to add a null byte and another to construct the GVariant),
  // we instead create a new backing buffer ourselves for the GVariant to use.

  // The serialized form of a string GVariant is just the string contents
  // followed by a null byte.
  char* data = static_cast<char*>(g_malloc(value.size() + 1));
  std::copy(value.begin(), value.end(), data);
  // SAFETY: We allocate |data| earlier in this function to have a size of
  // value.size() + 1.
  UNSAFE_BUFFERS(data[value.size()] = '\0');

  // GVariant holds the buffer as a reference-counted GBytes object. The GBytes
  // object takes ownership of |data|, and will call g_free on it when the last
  // reference is dropped.
  GBytes* bytes = g_bytes_new_take(data, value.size() + 1);

  // g_variant_new_from_bytes() creates a new GVariant using |bytes| as its
  // backing buffer without making a copy.
  GVariantRef<C> variant = GVariantRef<C>::TakeUnchecked(
      g_variant_new_from_bytes(C.gvariant_type(), bytes, true));

  // g_variant_new_from_bytes() takes its own ref to |bytes|.
  g_bytes_unref(bytes);

  return variant;
}

// static
auto Mapping<std::string>::From(const std::string& value)
    -> GVariantRef<kType> {
  return GVariantRef<kType>::From(std::string_view(value));
}

// static
auto Mapping<std::string>::TryFrom(const std::string& value)
    -> base::expected<GVariantRef<kType>, Loggable> {
  return GVariantRef<kType>::TryFrom(std::string_view(value));
}

// static
std::string Mapping<std::string>::Into(const GVariantRef<kType>& variant) {
  gsize length;
  const char* value = g_variant_get_string(variant.raw(), &length);
  return std::string(value, length);
}

// static
auto Mapping<std::string_view>::From(std::string_view value)
    -> GVariantRef<kType> {
  auto result = TryFrom(value);
  CHECK(result.has_value()) << result.error();
  return result.value();
}

// static
auto Mapping<std::string_view>::TryFrom(std::string_view value)
    -> base::expected<GVariantRef<kType>, Loggable> {
  if (!g_utf8_validate(value.data(), value.length(), nullptr)) {
    return base::unexpected(Loggable(FROM_HERE, "String is not valid UTF-8"));
  }

  return base::ok(CreateStringVariantUnchecked<kType>(value));
}

// static
auto Mapping<const char*>::From(const char* value) -> GVariantRef<kType> {
  return GVariantRef<kType>::From(std::string_view(value));
}

// static
auto Mapping<const char*>::TryFrom(const char* value)
    -> base::expected<GVariantRef<kType>, Loggable> {
  return GVariantRef<kType>::TryFrom(std::string_view(value));
}

// static
Ignored Mapping<Ignored>::Into(const GVariantRef<kType>& variant) {
  return Ignored();
}

// static
decltype(std::ignore) Mapping<decltype(std::ignore)>::Into(
    const GVariantRef<kType>& variant) {
  return std::ignore;
}

// static
auto Mapping<ObjectPathCStr>::From(const ObjectPathCStr& value)
    -> GVariantRef<kType> {
  return CreateStringVariantUnchecked<kType>(value.c_str());
}

// static
auto Mapping<ObjectPath>::From(const ObjectPath& value) -> GVariantRef<kType> {
  return CreateStringVariantUnchecked<kType>(value.value());
}

// static
ObjectPath Mapping<ObjectPath>::Into(const GVariantRef<kType>& variant) {
  gsize length;
  const char* value = g_variant_get_string(variant.raw(), &length);

  // Value is already guaranteed to be a valid object path.
  return ObjectPath(std::string(value, length));
}

// static
auto Mapping<TypeSignatureCStr>::From(const TypeSignatureCStr& value)
    -> GVariantRef<kType> {
  return CreateStringVariantUnchecked<kType>(value.c_str());
}

// static
auto Mapping<TypeSignature>::From(const TypeSignature& value)
    -> GVariantRef<kType> {
  return CreateStringVariantUnchecked<kType>(value.value());
}

// static
TypeSignature Mapping<TypeSignature>::Into(const GVariantRef<kType>& variant) {
  gsize length;
  const char* value = g_variant_get_string(variant.raw(), &length);

  // Value is already guaranteed to be a valid object path.
  return TypeSignature(std::string(value, length));
}

}  // namespace remoting::gvariant
