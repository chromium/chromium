// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_HEADERS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_HEADERS_H_

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/fetch/fetch_header_list.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ByteStringSequenceSequenceOrByteStringByteStringRecord;
class ExceptionState;

using HeadersInit = ByteStringSequenceSequenceOrByteStringByteStringRecord;

// http://fetch.spec.whatwg.org/#headers-class
class CORE_EXPORT Headers final : public ScriptWrappable,
                                  public PairIterable<String, String> {
  DEFINE_WRAPPERTYPEINFO();

 public:
  enum Guard {
    kImmutableGuard,
    kRequestGuard,
    kRequestNoCorsGuard,
    kResponseGuard,
    kNoneGuard
  };

  static Headers* Create(ExceptionState& exception_state);
  static Headers* Create(const V8HeadersInit* init,
                         ExceptionState& exception_state);

  // Shares the FetchHeaderList. Called when creating a Request or Response.
  static Headers* Create(FetchHeaderList*);

  Headers();
  // Shares the FetchHeaderList. Called when creating a Request or Response.
  explicit Headers(FetchHeaderList*);

  Headers* Clone() const;

  // Headers.idl implementation.
  void append(const String& name, const String& value, ExceptionState&);
  void remove(const String& key, ExceptionState&);
  String get(const String& key, ExceptionState&);
  bool has(const String& key, ExceptionState&);
  void set(const String& key, const String& value, ExceptionState&);

  void SetGuard(Guard guard) { guard_ = guard; }
  Guard GetGuard() const { return guard_; }

  // These methods should only be called when size() would return 0.
  void FillWith(const Headers*, ExceptionState&);
  void FillWith(const V8HeadersInit* init, ExceptionState& exception_state);

  // https://fetch.spec.whatwg.org/#concept-headers-remove-privileged-no-cors-request-headers
  void RemovePrivilegedNoCorsRequestHeaders();

  FetchHeaderList* HeaderList() const { return header_list_; }
  void Trace(Visitor*) const override;

 private:
  // These methods should only be called when size() would return 0.
  void FillWith(const Vector<Vector<String>>&, ExceptionState&);
  void FillWith(const Vector<std::pair<String, String>>&, ExceptionState&);

  Member<FetchHeaderList> header_list_;
  Guard guard_;

  IterationSource* StartIteration(ScriptState*, ExceptionState&) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FETCH_HEADERS_H_
