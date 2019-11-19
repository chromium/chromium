/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CRYPTO_SUBTLE_CRYPTO_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CRYPTO_SUBTLE_CRYPTO_H_

#include "third_party/blink/renderer/bindings/core/v8/array_buffer_or_array_buffer_view.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/array_buffer_or_array_buffer_view_or_dictionary.h"
#include "third_party/blink/renderer/bindings/modules/v8/dictionary_or_string.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class CryptoKey;

typedef ArrayBufferOrArrayBufferView BufferSource;
typedef DictionaryOrString AlgorithmIdentifier;

class SubtleCrypto final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  SubtleCrypto();

  ScriptPromise encrypt(ScriptState*,
                        const AlgorithmIdentifier&,
                        CryptoKey*,
                        const BufferSource&);
  ScriptPromise decrypt(ScriptState*,
                        const AlgorithmIdentifier&,
                        CryptoKey*,
                        const BufferSource&);
  ScriptPromise sign(ScriptState*,
                     const AlgorithmIdentifier&,
                     CryptoKey*,
                     const BufferSource&);
  // Note that this is not named "verify" because when compiling on Mac that
  // expands to a macro and breaks.
  ScriptPromise verifySignature(ScriptState*,
                                const AlgorithmIdentifier&,
                                CryptoKey*,
                                const BufferSource& signature,
                                const BufferSource& data);
  ScriptPromise digest(ScriptState*,
                       const AlgorithmIdentifier&,
                       const BufferSource& data);

  ScriptPromise generateKey(ScriptState*,
                            const AlgorithmIdentifier&,
                            bool extractable,
                            const Vector<String>& key_usages);
  ScriptPromise importKey(ScriptState*,
                          const String&,
                          const ArrayBufferOrArrayBufferViewOrDictionary&,
                          const AlgorithmIdentifier&,
                          bool extractable,
                          const Vector<String>& key_usages);
  ScriptPromise exportKey(ScriptState*, const String&, CryptoKey*);

  ScriptPromise wrapKey(ScriptState*,
                        const String&,
                        CryptoKey*,
                        CryptoKey*,
                        const AlgorithmIdentifier&);
  ScriptPromise unwrapKey(ScriptState*,
                          const String&,
                          const BufferSource&,
                          CryptoKey*,
                          const AlgorithmIdentifier&,
                          const AlgorithmIdentifier&,
                          bool,
                          const Vector<String>&);

  ScriptPromise deriveBits(ScriptState*,
                           const AlgorithmIdentifier&,
                           CryptoKey*,
                           unsigned);
  ScriptPromise deriveKey(ScriptState*,
                          const AlgorithmIdentifier&,
                          CryptoKey*,
                          const AlgorithmIdentifier&,
                          bool extractable,
                          const Vector<String>&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CRYPTO_SUBTLE_CRYPTO_H_
