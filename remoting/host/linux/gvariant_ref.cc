// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gvariant_ref.h"

#include <glib.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/types/expected.h"

namespace remoting::gvariant {

// GVariantRef implementation

GVariantBase::operator GVariantRef<>() const {
  return GVariantRef<>::Ref(raw());
}

GVariant* GVariantBase::raw() const {
  return variant_;
}

GVariant* GVariantBase::release() && {
  GVariant* ref = variant_;
  variant_ = nullptr;
  return ref;
}

Type<> GVariantBase::GetType() const {
  return Type(g_variant_get_type(variant_));
}

GVariantBase::GVariantBase() : variant_(nullptr) {}

GVariantBase::GVariantBase(decltype(kTake), GVariant* variant)
    : variant_(variant) {
  DCHECK(variant != nullptr);
  g_variant_take_ref(variant);
}

GVariantBase::GVariantBase(decltype(kRef), GVariant* variant)
    : variant_(variant) {
  DCHECK(variant != nullptr);
  g_variant_ref(variant);
}

GVariantBase::GVariantBase(decltype(kRefSink), GVariant* variant)
    : variant_(variant) {
  DCHECK(variant != nullptr);
  g_variant_ref_sink(variant);
}

GVariantBase::GVariantBase(const GVariantBase& other)
    : variant_(other.variant_) {
  if (variant_ != nullptr) {
    g_variant_ref(variant_);
  }
}

GVariantBase::GVariantBase(GVariantBase&& other) : variant_(other.variant_) {
  other.variant_ = nullptr;
}

GVariantBase& GVariantBase::operator=(const GVariantBase& other) {
  if (this == &other) {
    return *this;
  }

  if (variant_ != nullptr) {
    g_variant_unref(variant_);
  }

  variant_ = other.variant_;

  if (variant_ != nullptr) {
    g_variant_ref(variant_);
  }

  return *this;
}

GVariantBase& GVariantBase::operator=(GVariantBase&& other) {
  if (this == &other) {
    return *this;
  }

  if (variant_ != nullptr) {
    g_variant_unref(variant_);
  }

  variant_ = other.variant_;
  other.variant_ = nullptr;

  return *this;
}

GVariantBase::~GVariantBase() {
  if (variant_ != nullptr) {
    g_variant_unref(variant_.ExtractAsDangling());
  }
}

bool GVariantBase::operator==(const GVariantBase& other) const {
  return g_variant_equal(raw(), other.raw());
}

// GVariantRef implementation

// static
template <Type C>
GVariantRef<C> GVariantRef<C>::Take(GVariant* variant)
  requires(C == Type("*"))
{
  return GVariantRef<C>(kTake, variant);
}
template GVariantRef<Type("*")> GVariantRef<Type("*")>::Take(GVariant* variant);

// static
template <Type C>
GVariantRef<C> GVariantRef<C>::Ref(GVariant* variant)
  requires(C == Type("*"))
{
  return GVariantRef<C>(kRef, variant);
}
template GVariantRef<Type("*")> GVariantRef<Type("*")>::Ref(GVariant* variant);

// static
template <Type C>
GVariantRef<C> GVariantRef<C>::RefSink(GVariant* variant)
  requires(C == Type("*"))
{
  return GVariantRef<C>(kRefSink, variant);
}
template GVariantRef<Type("*")> GVariantRef<Type("*")>::RefSink(
    GVariant* variant);

// static
base::expected<ObjectPath, std::string> ObjectPath::TryFrom(std::string path) {
  if (g_variant_is_object_path(path.c_str())) {
    return base::ok(ObjectPath(std::move(path)));
  } else {
    return base::unexpected(
        base::StrCat({"String is not a valid object path: ", path}));
  }
}

// Wrapper implementations

const std::string& ObjectPath::value() const {
  return path_;
}

ObjectPath::ObjectPath(std::string path) : path_(path) {}

// static
base::expected<TypeSignature, std::string> TypeSignature::TryFrom(
    std::string signature) {
  if (g_variant_is_signature(signature.c_str())) {
    return base::ok(TypeSignature(std::move(signature)));
  } else {
    return base::unexpected(
        base::StrCat({"String is not a valid type signature: ", signature}));
  }
}

const std::string& TypeSignature::value() const {
  return signature_;
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

template <Type C>
static GVariantRef<C> CreateStringVariant(std::string_view value) {
  char* data = static_cast<char*>(g_malloc(value.size() + 1));
  std::copy(value.begin(), value.end(), data);
  data[value.size()] = '\0';
  GBytes* bytes = g_bytes_new_take(data, value.size() + 1);
  GVariantRef<C> variant = GVariantRef<C>::TakeUnchecked(
      g_variant_new_from_bytes(C.gvariant_type(), bytes, true));

  // g_variant_new_from_bytes takes its own ref to bytes.
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
    -> base::expected<GVariantRef<kType>, std::string> {
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
    -> base::expected<GVariantRef<kType>, std::string> {
  if (!g_utf8_validate(value.data(), value.length(), nullptr)) {
    return base::unexpected("String is not valid UTF-8");
  }

  return base::ok(CreateStringVariant<kType>(value));
}

// static
auto Mapping<const char*>::From(const char* value) -> GVariantRef<kType> {
  return GVariantRef<kType>::From(std::string_view(value));
}

// static
auto Mapping<const char*>::TryFrom(const char* value)
    -> base::expected<GVariantRef<kType>, std::string> {
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
auto Mapping<ObjectPath>::From(const ObjectPath& value) -> GVariantRef<kType> {
  return CreateStringVariant<kType>(value.value());
}

// static
ObjectPath Mapping<ObjectPath>::Into(const GVariantRef<kType>& variant) {
  gsize length;
  const char* value = g_variant_get_string(variant.raw(), &length);

  // Value is already guaranteed to be a valid object path.
  return ObjectPath(std::string(value, length));
}

// static
auto Mapping<TypeSignature>::From(const TypeSignature& value)
    -> GVariantRef<kType> {
  return CreateStringVariant<kType>(value.value());
}

// static
TypeSignature Mapping<TypeSignature>::Into(const GVariantRef<kType>& variant) {
  gsize length;
  const char* value = g_variant_get_string(variant.raw(), &length);

  // Value is already guaranteed to be a valid object path.
  return TypeSignature(std::string(value, length));
}

}  // namespace remoting::gvariant
