// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DICTIONARY_BASE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DICTIONARY_BASE_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
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

  v8::Local<v8::Value> ToV8(ScriptState* script_state) const;

  virtual void Trace(Visitor*) const {}

 protected:
  DictionaryBase() = default;

  DictionaryBase(const DictionaryBase&) = delete;
  DictionaryBase(const DictionaryBase&&) = delete;
  DictionaryBase& operator=(const DictionaryBase&) = delete;
  DictionaryBase& operator=(const DictionaryBase&&) = delete;

  virtual const void* TemplateKey() const = 0;
  virtual void FillTemplateProperties(
      WTF::Vector<std::string_view>& properties) const = 0;
  virtual v8::Local<v8::Object> FillValues(
      ScriptState* script_state,
      v8::Local<v8::DictionaryTemplate> dict_template) const = 0;
};

}  // namespace bindings
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_DICTIONARY_BASE_H_
