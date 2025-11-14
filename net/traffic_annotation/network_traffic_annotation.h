// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TRAFFIC_ANNOTATION_NETWORK_TRAFFIC_ANNOTATION_H_
#define NET_TRAFFIC_ANNOTATION_NETWORK_TRAFFIC_ANNOTATION_H_

#include <cstdint>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/notreached.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/scoped_java_ref.h"
#endif

namespace net::internal {

template <size_t N>
consteval int32_t ComputeAnnotationHash(const char (&str)[N]) {
  uint32_t ret = 0;
  // - 1 to not include NUL
  for (size_t i = 0; i < N - 1; ++i) {
    ret = (ret * 31u + static_cast<uint32_t>(UNSAFE_TODO(str[i]))) % 138003713u;
  }
  return static_cast<int32_t>(ret);
}

// NOLINTBEGIN(google-explicit-constructor)
struct StringLiteralToHash {
  int32_t hash_value;

  template <size_t N>
  consteval StringLiteralToHash(const char (&str)[N]) {
    hash_value = ComputeAnnotationHash(str);
  }
};
// NOLINTEND(google-explicit-constructor)

constexpr int TRAFFIC_ANNOTATION_UNINITIALIZED = -1;
constexpr int32_t TEST_PARTIAL_HASH = ComputeAnnotationHash("test_partial");
constexpr int32_t UNDEFINED_HASH = ComputeAnnotationHash("undefined");
}  // namespace net::internal

namespace net {
struct PartialNetworkTrafficAnnotationTag;

// Defined types for network traffic annotation tags.
struct NetworkTrafficAnnotationTag {
  const int32_t unique_id_hash_code;

  bool operator==(const NetworkTrafficAnnotationTag& other) const {
    return unique_id_hash_code == other.unique_id_hash_code;
  }

  static NetworkTrafficAnnotationTag NotReached() { NOTREACHED(); }

  // These functions are wrappers around the (private) constructor, so we can
  // easily find the constructor's call-sites with a script.
  friend constexpr NetworkTrafficAnnotationTag DefineNetworkTrafficAnnotation(
      internal::StringLiteralToHash unique_id,
      const char* proto);

  friend NetworkTrafficAnnotationTag CompleteNetworkTrafficAnnotation(
      internal::StringLiteralToHash unique_id,
      const PartialNetworkTrafficAnnotationTag& partial_annotation,
      const char* proto);

  friend constexpr NetworkTrafficAnnotationTag
  BranchedCompleteNetworkTrafficAnnotation(
      internal::StringLiteralToHash unique_id,
      internal::StringLiteralToHash group_id,
      const PartialNetworkTrafficAnnotationTag& partial_annotation,
      const char* proto);

#if BUILDFLAG(IS_ANDROID)
  // Allows C++ methods to receive a Java NetworkTrafficAnnotationTag via JNI,
  // and convert it to the C++ version.
  static NetworkTrafficAnnotationTag FromJavaAnnotation(
      int32_t unique_id_hash_code);
#endif

  friend struct MutableNetworkTrafficAnnotationTag;

 private:
  constexpr explicit NetworkTrafficAnnotationTag(int32_t unique_id_hash_code_)
      : unique_id_hash_code(unique_id_hash_code_) {}
};

struct PartialNetworkTrafficAnnotationTag {
  const int32_t unique_id_hash_code;

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  // |completing_id_hash_code| holds a reference to the hash coded unique id
  // of a network traffic annotation (or group id of several network traffic
  // annotations) that complete a partial network annotation. Please refer to
  // the description of DefinePartialNetworkTrafficAnnotation function for more
  // details.
  // This value is used by the clang tools to find linkage between partial
  // annotations and their completing parts, and is used in debug mode to check
  // if an intended completing part is added to a partial network annotation.
  const int32_t completing_id_hash_code;
#endif

  // This function is a wrapper around the (private) constructor, so we can
  // easily find the constructor's call-sites with a script.
  friend constexpr PartialNetworkTrafficAnnotationTag
  DefinePartialNetworkTrafficAnnotation(
      internal::StringLiteralToHash unique_id,
      internal::StringLiteralToHash completing_id,
      const char* proto);

  friend struct MutablePartialNetworkTrafficAnnotationTag;

 private:
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  constexpr PartialNetworkTrafficAnnotationTag(int32_t unique_id_hash_code_,
                                               int32_t completing_id_hash_code_)
      : unique_id_hash_code(unique_id_hash_code_),
        completing_id_hash_code(completing_id_hash_code_) {}
#else
  constexpr explicit PartialNetworkTrafficAnnotationTag(
      int32_t unique_id_hash_code_)
      : unique_id_hash_code(unique_id_hash_code_) {}
#endif
};

// Function to convert a network traffic annotation's unique id and protobuf
// text into a NetworkTrafficAnnotationTag.
//
// This function serves as a tag that can be discovered and extracted via
// clang tools. This allows reviewing all network traffic that is generated
// and annotated by Chrome.
//
// |unique_id| should be a string that uniquely identifies this annotation
// across all of Chromium source code. |unique_id| should be kept unchanged
// as long as possible as its hashed value will be used for differnt logging,
// debugging, or auditing tasks. Unique ids should include only alphanumeric
// characters and underline.
// |proto| is a text-encoded NetworkTrafficAnnotation protobuf (see
// chrome/browser/privacy/traffic_annotation.proto)
//
// An empty and a sample template for the text-encoded protobuf can be found in
// tools/traffic_annotation/sample_traffic_annotation.cc.
// TODO(crbug.com/40505662): Add tools to check annotation text's format during
// presubmit checks.
inline constexpr NetworkTrafficAnnotationTag DefineNetworkTrafficAnnotation(
    internal::StringLiteralToHash unique_id,
    const char* proto) {
  return NetworkTrafficAnnotationTag(unique_id.hash_value);
}

// There are cases where the network traffic annotation cannot be fully
// specified in one place. For example, in one place we know the trigger of a
// network request and in another place we know the data that will be sent. In
// these cases, we prefer that both parts of the annotation appear in context so
// that they are updated if code changes. The following functions help splitting
// the network traffic annotation into two pieces. Please refer to
// tools/traffic_annotation/sample_traffic_annotation.cc for usage samples.

// This function can be used to define a partial annotation that will be
// completed later. The completing annotation can be defined with either of
// 'CompleteNetworkTrafficAnnotation' or
// 'BranchedCompleteNetworkTrafficAnnotation' functions. In case of
// CompleteNetworkTrafficAnnotation, |completing_id| is the unique id of the
// annotation that will complete it. In the case of
// BranchedCompleteNetworkTrafficAnnotation, |completing_id| is the group id
// of the completing annotations.
constexpr PartialNetworkTrafficAnnotationTag
DefinePartialNetworkTrafficAnnotation(
    internal::StringLiteralToHash unique_id,
    internal::StringLiteralToHash completing_id,
    const char* proto) {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  return PartialNetworkTrafficAnnotationTag(unique_id.hash_value,
                                            completing_id.hash_value);
#else
  return PartialNetworkTrafficAnnotationTag(unique_id.hash_value);
#endif
}

// This function can be used to define a completing partial annotation. This
// annotation adds details to another annotation that is defined before.
// |partial_annotation| is the PartialNetworkTrafficAnnotationTag returned
// by a call to DefinePartialNetworkTrafficAnnotation().
inline NetworkTrafficAnnotationTag CompleteNetworkTrafficAnnotation(
    internal::StringLiteralToHash unique_id,
    const PartialNetworkTrafficAnnotationTag& partial_annotation,
    const char* proto) {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  DCHECK(partial_annotation.completing_id_hash_code == unique_id.hash_value ||
         partial_annotation.unique_id_hash_code ==
             internal::TEST_PARTIAL_HASH ||
         partial_annotation.unique_id_hash_code == internal::UNDEFINED_HASH);
#endif
  return NetworkTrafficAnnotationTag(partial_annotation.unique_id_hash_code);
}

// This function can be used to define a completing partial annotation that is
// branched into several annotations. In this case, |group_id| is a common id
// that is used by all members of the branch and referenced by partial
// annotation that is completed by them.
constexpr inline NetworkTrafficAnnotationTag
BranchedCompleteNetworkTrafficAnnotation(
    internal::StringLiteralToHash unique_id,
    internal::StringLiteralToHash group_id,
    const PartialNetworkTrafficAnnotationTag& partial_annotation,
    const char* proto) {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  DCHECK(partial_annotation.completing_id_hash_code == group_id.hash_value ||
         partial_annotation.unique_id_hash_code ==
             internal::TEST_PARTIAL_HASH ||
         partial_annotation.unique_id_hash_code == internal::UNDEFINED_HASH);
#endif
  return NetworkTrafficAnnotationTag(unique_id.hash_value);
}

// Example for joining N x 1 partial annotations:
// N functions foo1(), ..., fooN() call one function bar(). Each
// foo...() function defines part of a network traffic annotation.
// These N partial annotations are combined with a second part in
// bar().
//
// void foo1() {
//   auto tag = DefinePartialNetworkTrafficAnnotation(
//       "call_by_foo1", "completion_by_bar", [partial_proto]);
//   bar(tag);
// }
// void foo2() {
//   auto tag = DefinePartialNetworkTrafficAnnotation(
//       "call_by_foo2", "completion_by_bar", [partial_proto]);
//   bar(tag);
// }
// void bar(PartialNetworkTrafficAnnotationTag tag) {
//   auto final_tag = CompleteNetworkTrafficAnnotation(
//     "completion_by_bar", tag, [rest_of_proto]);
//   // final_tag matches the value of tag (which is hash code of
//   // "call_by_fooX" where X can be 1 or 2).
//   net::URLFetcher::Create(..., final_tag);
// }

// Example for joining 1 x N partial annotations:
// A function foo() calls a function bar(bool param), that sends
// different network requests depending on param. Both functions
// define parts of the network traffic annotation.
//
// void foo(bool param) {
//   auto tag = DefinePartialNetworkTrafficAnnotation(
//       "call_by_foo1", "completion_by_bar", [partial_proto]);
//   bar(param, tag);
// }
// void bar(bool param, PartialNetworkTrafficAnnotationTag tag) {
//   if (param) {
//     auto final_tag = BranchedCompleteNetworkTrafficAnnotation(
//       "call_bool_branch_1", "completion_by_bar", tag, [rest_of_proto]);
//     // final_tag is hash code of "call_bool_branch_1".
//     net::URLFetcher::Create(url1, ..., final_tag);
//   } else {
//     auto final_tag = BranchedCompleteNetworkTrafficAnnotation(
//       "call_bool_branch_2", "completion_by_bar", tag, [rest_of_proto]);
//     // final_tag is hash code of "call_bool_branch_2".
//     net::URLFetcher::Create(url2, ..., final_tag);
//   }
// }

// Please do not use this unless uninitialized annotations are required.
// Mojo interfaces for this class and the next one are defined in
// '/services/network/public/mojom'.
struct MutableNetworkTrafficAnnotationTag {
  MutableNetworkTrafficAnnotationTag()
      : unique_id_hash_code(internal::TRAFFIC_ANNOTATION_UNINITIALIZED) {}
  explicit MutableNetworkTrafficAnnotationTag(
      const NetworkTrafficAnnotationTag& traffic_annotation)
      : unique_id_hash_code(traffic_annotation.unique_id_hash_code) {}

  int32_t unique_id_hash_code;

  bool operator==(const MutableNetworkTrafficAnnotationTag& other) const {
    return unique_id_hash_code == other.unique_id_hash_code;
  }

  explicit operator NetworkTrafficAnnotationTag() const {
    DCHECK(is_valid());
    return NetworkTrafficAnnotationTag(unique_id_hash_code);
  }

  bool is_valid() const {
    return unique_id_hash_code != internal::TRAFFIC_ANNOTATION_UNINITIALIZED;
  }

  void reset() {
    unique_id_hash_code = internal::TRAFFIC_ANNOTATION_UNINITIALIZED;
  }

  // This function is a wrapper around the private constructor, so we can easily
  // find the constructor's call-sites with a script.
  friend MutableNetworkTrafficAnnotationTag
  CreateMutableNetworkTrafficAnnotationTag(int32_t unique_id_hash_code);

 private:
  explicit MutableNetworkTrafficAnnotationTag(int32_t unique_id_hash_code_)
      : unique_id_hash_code(unique_id_hash_code_) {}
};

inline MutableNetworkTrafficAnnotationTag
CreateMutableNetworkTrafficAnnotationTag(int32_t unique_id_hash_code) {
  return MutableNetworkTrafficAnnotationTag(unique_id_hash_code);
}

struct MutablePartialNetworkTrafficAnnotationTag {
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
  MutablePartialNetworkTrafficAnnotationTag()
      : unique_id_hash_code(internal::TRAFFIC_ANNOTATION_UNINITIALIZED),
        completing_id_hash_code(internal::TRAFFIC_ANNOTATION_UNINITIALIZED) {}
  explicit MutablePartialNetworkTrafficAnnotationTag(
      const PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      : unique_id_hash_code(partial_traffic_annotation.unique_id_hash_code),
        completing_id_hash_code(
            partial_traffic_annotation.completing_id_hash_code) {}

  int32_t unique_id_hash_code;
  int32_t completing_id_hash_code;

  explicit operator PartialNetworkTrafficAnnotationTag() const {
    DCHECK(is_valid());
    return PartialNetworkTrafficAnnotationTag(unique_id_hash_code,
                                              completing_id_hash_code);
  }

  bool is_valid() const {
    return unique_id_hash_code != internal::TRAFFIC_ANNOTATION_UNINITIALIZED &&
           completing_id_hash_code !=
               internal::TRAFFIC_ANNOTATION_UNINITIALIZED;
  }

  void reset() {
    unique_id_hash_code = internal::TRAFFIC_ANNOTATION_UNINITIALIZED;
    completing_id_hash_code = internal::TRAFFIC_ANNOTATION_UNINITIALIZED;
  }
#else
  MutablePartialNetworkTrafficAnnotationTag()
      : unique_id_hash_code(internal::TRAFFIC_ANNOTATION_UNINITIALIZED) {}
  explicit MutablePartialNetworkTrafficAnnotationTag(
      const PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      : unique_id_hash_code(partial_traffic_annotation.unique_id_hash_code) {}

  int32_t unique_id_hash_code;

  explicit operator PartialNetworkTrafficAnnotationTag() const {
    return PartialNetworkTrafficAnnotationTag(unique_id_hash_code);
  }

  bool is_valid() const {
    return unique_id_hash_code != internal::TRAFFIC_ANNOTATION_UNINITIALIZED;
  }

  void reset() {
    unique_id_hash_code = internal::TRAFFIC_ANNOTATION_UNINITIALIZED;
  }
#endif  // !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
};

}  // namespace net

// Placeholder for unannotated usages.
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CHROMEOS)
#define TRAFFIC_ANNOTATION_WITHOUT_PROTO(ANNOTATION_ID) \
  net::DefineNetworkTrafficAnnotation(ANNOTATION_ID, "No proto yet.")
#endif

// These annotations are unavailable on desktop Linux + Windows. They are
// available on other platforms, since we only audit network annotations on
// Linux & Windows.
//
// On Linux and Windows, use MISSING_TRAFFIC_ANNOTATION or
// TRAFFIC_ANNOTATION_FOR_TESTS.
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_LINUX)

#define NO_TRAFFIC_ANNOTATION_YET \
  net::DefineNetworkTrafficAnnotation("undefined", "Nothing here yet.")

#endif

#define MISSING_TRAFFIC_ANNOTATION     \
  net::DefineNetworkTrafficAnnotation( \
      "missing", "Function called without traffic annotation.")

#endif  // NET_TRAFFIC_ANNOTATION_NETWORK_TRAFFIC_ANNOTATION_H_
