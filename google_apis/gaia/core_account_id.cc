// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/core_account_id.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "build/build_config.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "google_apis/gaia/gaia_id.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_string.h"
#include "google_apis/gaia/android/jni_headers/CoreAccountId_jni.h"
#endif

namespace {
// Returns whether the string looks like an email (the test is
// crude an only checks whether it includes an '@').
bool IsEmailString(const std::string& string) {
  return base::Contains(string, '@');
}
}  // anonymous namespace

CoreAccountId::CoreAccountId() = default;

CoreAccountId::CoreAccountId(const CoreAccountId&) = default;

CoreAccountId::CoreAccountId(CoreAccountId&&) noexcept = default;

CoreAccountId::~CoreAccountId() = default;

CoreAccountId& CoreAccountId::operator=(const CoreAccountId&) = default;

CoreAccountId& CoreAccountId::operator=(CoreAccountId&&) noexcept = default;

// static
CoreAccountId CoreAccountId::FromGaiaId(const GaiaId& gaia_id) {
  if (gaia_id.empty())
    return CoreAccountId();

  DCHECK(!IsEmailString(gaia_id.ToString()))
      << "Expected a Gaia ID and got an email [actual = " << gaia_id << "]";
  return CoreAccountId::FromString(gaia_id.ToString());
}

// static
CoreAccountId CoreAccountId::FromRobotEmail(const std::string& robot_email) {
  if (robot_email.empty())
    return CoreAccountId();
  DCHECK(gaia::IsGoogleRobotAccountEmail(robot_email))
      << "Not a valid robot email [robot_email = " << robot_email << "]";
  return CoreAccountId::FromString(robot_email);
}

#if BUILDFLAG(IS_CHROMEOS)
// static
CoreAccountId CoreAccountId::FromEmail(const std::string& email) {
  if (email.empty())
    return CoreAccountId();

  DCHECK(IsEmailString(email))
      << "Expected an email [actual = " << email << "]";
  return CoreAccountId::FromString(email);
}
#endif

// static
CoreAccountId CoreAccountId::FromString(const std::string& value) {
  CoreAccountId account_id;
  account_id.id_ = value;
  return account_id;
}

bool CoreAccountId::empty() const {
  return id_.empty();
}

bool CoreAccountId::IsEmail() const {
  return IsEmailString(id_);
}

const std::string& CoreAccountId::ToString() const {
  return id_;
}

std::vector<std::string> ToStringList(
    const std::vector<CoreAccountId>& account_ids) {
  return base::ToVector(account_ids, &CoreAccountId::ToString);
}

#if BUILDFLAG(IS_ANDROID)
base::android::ScopedJavaLocalRef<jobject> ConvertToJavaCoreAccountId(
    JNIEnv* env,
    const CoreAccountId& account_id) {
  CHECK(!account_id.empty());
  return Java_CoreAccountId_Constructor(env, GaiaId(account_id.ToString()));
}

// Constructs a C++ CoreAccountId from the provided Java CoreAccountId.
CoreAccountId ConvertFromJavaCoreAccountId(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& j_core_account_id) {
  CHECK(j_core_account_id);
  return CoreAccountId::FromGaiaId(
      Java_CoreAccountId_getId(env, j_core_account_id));
}
#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
DEFINE_JNI(CoreAccountId)
#endif
