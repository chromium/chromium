// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_promise.h"

namespace media {

const CdmPromise::ResolveParameterType CdmPromiseTraits<>::kType =
    CdmPromise::VOID_TYPE;

const CdmPromise::ResolveParameterType CdmPromiseTraits<int>::kType =
    CdmPromise::INT_TYPE;

const CdmPromise::ResolveParameterType CdmPromiseTraits<std::string>::kType =
    CdmPromise::STRING_TYPE;

const CdmPromise::ResolveParameterType
    CdmPromiseTraits<CdmKeyInformation::KeyStatus>::kType =
        CdmPromise::KEY_STATUS_TYPE;

template <>
CdmPromise::ResolveParameterType CdmPromiseTemplate<>::GetResolveParameterType()
    const {
  return CdmPromiseTraits<>::kType;
}

template <>
CdmPromise::ResolveParameterType
CdmPromiseTemplate<int>::GetResolveParameterType() const {
  return CdmPromiseTraits<int>::kType;
}

template <>
CdmPromise::ResolveParameterType
CdmPromiseTemplate<std::string>::GetResolveParameterType() const {
  return CdmPromiseTraits<std::string>::kType;
}

template <>
CdmPromise::ResolveParameterType CdmPromiseTemplate<
    CdmKeyInformation::KeyStatus>::GetResolveParameterType() const {
  return CdmPromiseTraits<CdmKeyInformation::KeyStatus>::kType;
}

}  // namespace media
