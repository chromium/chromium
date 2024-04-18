// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SESSION_HASH_UTIL_H_
#define IOS_WEB_SESSION_HASH_UTIL_H_

#include <functional>
#include <string_view>
#include <tuple>

#include "net/base/hash_value.h"
#include "net/cert/x509_certificate.h"

namespace web {
namespace session {
namespace internal {

// Helper struct used to implement hash for session objects.
template <typename T>
struct Hasher : std::hash<T> {};

// Specialisation of Hasher for non-const pointers.
template <typename T>
struct Hasher<T*> : Hasher<const T*> {};

// Specialisation of Hasher for SHA256HashValue.
template <>
struct Hasher<net::SHA256HashValue> {
  size_t operator()(const net::SHA256HashValue& value) const {
    const std::string_view value_string_piece(
        reinterpret_cast<const char*>(&value.data[0]), sizeof(value.data));
    return Hasher<std::string_view>{}(value_string_piece);
  }
};

// Specialisation of Hasher for X509Certificate.
template <>
struct Hasher<const net::X509Certificate*> {
  size_t operator()(const net::X509Certificate* value) const {
    const net::SHA256HashValue fingerprint =
        value->CalculateChainFingerprint256();
    return Hasher<net::SHA256HashValue>{}(fingerprint);
  }
};

// Specialisation of Hasher for scoped_refptr<T>.
template <typename T>
struct Hasher<scoped_refptr<T>> {
  size_t operator()(const scoped_refptr<T>& value) const {
    return Hasher<std::add_pointer_t<std::decay_t<T>>>{}(value.get());
  }
};

// Combine `hash` with the hash of `value`.
template <typename T>
size_t CombineHash(size_t hash, T&& value) {
  const size_t value_hash = Hasher<std::decay_t<T>>{}(std::forward<T>(value));
  return hash ^ (value_hash + 0xeeffccdd + (hash << 5) + (hash >> 3));
}

// Helper to compute the hash of a tuple (end recursion case).
template <typename... T>
size_t TupleHasherImpl(size_t hash) {
  return hash;
}

// Helper to compute the hash of a tuple (general recursive case).
template <typename Head, typename... Tail>
size_t TupleHasherImpl(size_t hash, Head&& head, Tail&&... tail) {
  size_t curr = CombineHash(hash, std::forward<Head>(head));
  return TupleHasherImpl(curr, std::forward<Tail>(tail)...);
}

// Helper to compute the hash of a tuple (setup the recursion).
template <typename Tuple, size_t... I>
size_t TupleHasher(Tuple&& tuple, std::index_sequence<I...>) {
  return TupleHasherImpl(0, std::get<I>(tuple)...);
}

// Specialisation of Hasher for tuples.
template <typename... T>
struct Hasher<std::tuple<T...>> {
  size_t operator()(std::tuple<T...>&& value) const {
    return TupleHasher(
        std::forward<std::tuple<T...>>(value),
        std::make_index_sequence<std::tuple_size_v<std::tuple<T...>>>());
  }
};

}  // namespace internal

// Helper function computing the hash of a variadic number of args.
template <typename... Args>
size_t ComputeHash(Args&&... args) {
  using Hasher = internal::Hasher<std::tuple<Args...>>;
  return Hasher{}(std::tuple<Args...>(std::forward<Args>(args)...));
}

}  // namespace session
}  // namespace web

#endif  // IOS_WEB_SESSION_HASH_UTIL_H_
