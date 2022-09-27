// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DICTIONARY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DICTIONARY_BASE_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-local-handle.h"

namespace blink {

class ScriptState;

namespace bindings {

// DictionaryBase is the common base class of all the IDL dictionary classes.
// Most importantly this class provides a way of type dispatching (e.g. overload
// resolutions, SFINAE technique, etc.) so that it's possible to distinguish
// IDL dictionaries from anything else.  Also it provides a common
// implementation of IDL dictionaries.
class PLATFORM_EXPORT DictionaryBase : public GarbageCollected<DictionaryBase> {
 public:
  virtual ~DictionaryBase() = default;

  v8::MaybeLocal<v8::Value> ToV8Value(ScriptState* script_state) const;

  virtual void Trace(Visitor*) const {}

 protected:
  DictionaryBase() = default;

  DictionaryBase(const DictionaryBase&) = delete;
  DictionaryBase(const DictionaryBase&&) = delete;
  DictionaryBase& operator=(const DictionaryBase&) = delete;
  DictionaryBase& operator=(const DictionaryBase&&) = delete;

  // Fills the given v8::Object with the dictionary members.  Returns true on
  // success, otherwise returns false with throwing an exception.
  virtual bool FillV8ObjectWithMembers(
      ScriptState* script_state,
      v8::Local<v8::Object> v8_dictionary) const = 0;
};

}  // namespace bindings
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DICTIONARY_BASE_H_
