// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ORIGIN_TRIAL_FEATURES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ORIGIN_TRIAL_FEATURES_H_

#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "v8/include/v8.h"

namespace blink {

enum class OriginTrialFeature;
class ScriptState;
struct WrapperTypeInfo;

using InstallOriginTrialFeaturesFunction = void (*)(const WrapperTypeInfo*,
                                                    const ScriptState*,
                                                    v8::Local<v8::Object>,
                                                    v8::Local<v8::Function>);

using InstallPendingOriginTrialFeatureFunction = void (*)(OriginTrialFeature,
                                                          const ScriptState*);

// Sets the function to be called by |InstallOriginTrialFeatures|. The function
// is initially set to the private |InstallOriginTrialFeaturesDefault| function,
// but can be overridden by this function. A pointer to the previously set
// function is returned, so that functions can be chained.
PLATFORM_EXPORT InstallOriginTrialFeaturesFunction
    SetInstallOriginTrialFeaturesFunction(InstallOriginTrialFeaturesFunction);

// Sets the function to be called by |InstallPendingOriginTrialFeature|. This
// is initially set to the private |InstallPendingOriginTrialFeatureDefault|
// function, but can be overridden by this function. A pointer to the previously
// set function is returned, so that functions can be chained.
PLATFORM_EXPORT InstallPendingOriginTrialFeatureFunction
    SetInstallPendingOriginTrialFeatureFunction(
        InstallPendingOriginTrialFeatureFunction);

// Installs all of the conditionally enabled V8 bindings for the given type, in
// a specific context. This is called in V8PerContextData, after the constructor
// and prototype for the type have been created. It indirectly calls the
// function set by |SetInstallOriginTrialFeaturesFunction|.
PLATFORM_EXPORT void InstallOriginTrialFeatures(const WrapperTypeInfo*,
                                                const ScriptState*,
                                                v8::Local<v8::Object>,
                                                v8::Local<v8::Function>);

// Installs all of the conditionally enabled V8 bindings for a feature, if
// needed. This is called to install a newly-enabled feature on any existing
// objects. If the target object hasn't been created, nothing is installed. The
// enabled feature will be instead be installed when the object is created
// (avoids forcing the creation of objects prematurely).
PLATFORM_EXPORT void InstallPendingOriginTrialFeature(OriginTrialFeature,
                                                      const ScriptState*);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_ORIGIN_TRIAL_FEATURES_H_
