// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/webcrypto/webcrypto_impl.h"

#include <limits.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/no_destructor.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_runner.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "components/webcrypto/algorithm_dispatch.h"
#include "components/webcrypto/generate_key_result.h"
#include "components/webcrypto/status.h"
#include "third_party/blink/public/platform/web_crypto_key_algorithm.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_crypto_histograms.h"

namespace webcrypto {

using webcrypto::Status;

namespace {

// ---------------------
// Threading
// ---------------------
//
// WebCrypto operations can be slow. For instance generating an RSA key can
// take seconds.
//
// The strategy used here is to run a worker pool for all WebCrypto operations
// (except structured cloning). This same pool is also used by requests started
// from Blink Web Workers.
//
// A few notes to keep in mind:
//
// * PostTaskAndReply() is not used because of how it handles failures -- it
//   leaks the callback when failing to post back to the origin thread.
//
//   This is a problem since WebCrypto may be called from WebWorker threads,
//   which may be aborted at any time. Leaking would be undesirable, and
//   reachable in practice.
//
// * blink::WebArrayBuffer is NOT threadsafe, and should therefore be allocated
//   only on the target Blink thread.
//
//   TODO(eroman): Is there any way around this? Copying the result between
//                 threads is silly.
//
// * WebCryptoAlgorithm and WebCryptoKey are threadsafe, by virtue of being
//   immutable. Internally asymmetric WebCryptoKeys wrap BoringSSL's EVP_PKEY.
//   These are safe to use for BoringSSL operations across threads, provided
//   the internals of the EVP_PKEY are not mutated (they never should be
//   following ImportKey()).
//
// * blink::WebCryptoResult is not threadsafe and should only be operated on
//   the target Blink thread. HOWEVER, it is safe to delete it from any thread.
//   This can happen if by the time the operation has completed in the crypto
//   worker pool, the Blink worker thread that initiated the request is gone.
//   Posting back to the origin thread will fail, and the WebCryptoResult will
//   be deleted while running in the crypto worker pool.
class CryptoThreadPool {
 public:
  CryptoThreadPool() : worker_thread_("WebCrypto") {
    base::Thread::Options options;
    options.joinable = false;
    worker_thread_.StartWithOptions(std::move(options));
  }

  CryptoThreadPool(const CryptoThreadPool&) = delete;
  CryptoThreadPool& operator=(const CryptoThreadPool&) = delete;

  static bool PostTask(const base::Location& from_here, base::OnceClosure task);

 private:
  // TODO(gab): the pool is currently using a single non-joinable thread to
  // mimic the old behavior of using a CONTINUE_ON_SHUTDOWN SequencedTaskRunner
  // on a single-threaded SequencedWorkerPool, but we'd like to consider using
  // the ThreadPool here and allowing multiple threads (SEQUENCED or even
  // PARALLEL ExecutionMode: http://crbug.com/623700).
  base::Thread worker_thread_;
};

bool CryptoThreadPool::PostTask(const base::Location& from_here,
                                base::OnceClosure task) {
  static base::NoDestructor<CryptoThreadPool> crypto_thread_pool;
  return crypto_thread_pool->worker_thread_.task_runner()->PostTask(
      from_here, std::move(task));
}

void CompleteWithThreadPoolError(blink::WebCryptoResult* result) {
  result->CompleteWithError(blink::kWebCryptoErrorTypeOperation,
                            "Failed posting to crypto worker pool");
}

void CompleteWithError(const Status& status, blink::WebCryptoResult* result) {
  DCHECK(status.IsError());

  result->CompleteWithError(status.error_type(),
                            blink::WebString::FromUTF8(status.error_details()));
}

void CompleteWithBufferOrError(const Status& status,
                               const std::vector<uint8_t>& buffer,
                               blink::WebCryptoResult* result) {
  if (status.IsError()) {
    CompleteWithError(status, result);
  } else if (buffer.size() > UINT_MAX) {
    // WebArrayBuffers have a smaller range than std::vector<>, so
    // theoretically this could overflow.
    CompleteWithError(Status::ErrorUnexpected(), result);
  } else {
    result->CompleteWithBuffer(buffer);
  }
}

void CompleteWithKeyOrError(const Status& status,
                            const blink::WebCryptoKey& key,
                            blink::WebCryptoResult* result) {
  if (status.IsError()) {
    CompleteWithError(status, result);
  } else {
    result->CompleteWithKey(key);
  }
}

// --------------------------------------------------------------------
// State
// --------------------------------------------------------------------
//
// Explicit state classes are used rather than base::Bind{Once,Repeating}. This
// is done both for clarity, but also to avoid extraneous allocations for things
// like passing buffers and result objects between threads.
//
// BaseState is the base class common to all of the async operations, and
// keeps track of the thread to complete on, the error state, and the
// callback into Blink.
//
// Ownership of the State object is passed between the crypto thread and the
// Blink thread. Under normal completion it is destroyed on the Blink thread.
// However it may also be destroyed on the crypto thread if the Blink thread
// has vanished (which can happen for Blink web worker threads).

struct BaseState {
  BaseState(const blink::WebCryptoResult& result,
            scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : origin_thread(task_runner), result(result) {}

  bool cancelled() { return result.Cancelled(); }

  scoped_refptr<base::TaskRunner> origin_thread;

  webcrypto::Status status;
  blink::WebCryptoResult result;

 protected:
  // Since there is no virtual destructor, must not delete directly as a
  // BaseState.
  ~BaseState() {}
};

struct EncryptState : public BaseState {
  EncryptState(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               blink::WebVector<unsigned char> data,
               const blink::WebCryptoResult& result,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : BaseState(result, std::move(task_runner)),
        algorithm(algorithm),
        key(key),
        data(std::move(data)) {}

  const blink::WebCryptoAlgorithm algorithm;
  const blink::WebCryptoKey key;
  const blink::WebVector<unsigned char> data;

  std::vector<uint8_t> buffer;
};

typedef EncryptState DecryptState;
typedef EncryptState DigestState;

struct GenerateKeyState : public BaseState {
  GenerateKeyState(const blink::WebCryptoAlgorithm& algorithm,
                   bool extractable,
                   blink::WebCryptoKeyUsageMask usages,
                   const blink::WebCryptoResult& result,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : BaseState(result, std::move(task_runner)),
        algorithm(algorithm),
        extractable(extractable),
        usages(usages) {}

  const blink::WebCryptoAlgorithm algorithm;
  const bool extractable;
  const blink::WebCryptoKeyUsageMask usages;

  webcrypto::GenerateKeyResult generate_key_result;
};

struct ImportKeyState : public BaseState {
  ImportKeyState(blink::WebCryptoKeyFormat format,
                 blink::WebVector<unsigned char> key_data,
                 const blink::WebCryptoAlgorithm& algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 const blink::WebCryptoResult& result,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : BaseState(result, std::move(task_runner)),
        format(format),
        key_data(std::move(key_data)),
        algorithm(algorithm),
        extractable(extractable),
        usages(usages) {}

  const blink::WebCryptoKeyFormat format;
  const blink::WebVector<unsigned char> key_data;
  const blink::WebCryptoAlgorithm algorithm;
  const bool extractable;
  const blink::WebCryptoKeyUsageMask usages;

  blink::WebCryptoKey key;
};

struct ExportKeyState : public BaseState {
  ExportKeyState(blink::WebCryptoKeyFormat format,
                 const blink::WebCryptoKey& key,
                 const blink::WebCryptoResult& result,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : BaseState(result, std::move(task_runner)), format(format), key(key) {}

  const blink::WebCryptoKeyFormat format;
  const blink::WebCryptoKey key;

  std::vector<uint8_t> buffer;
};

typedef EncryptState SignState;

struct VerifySignatureState : public BaseState {
  VerifySignatureState(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& key,
                       blink::WebVector<unsigned char> signature,
                       blink::WebVector<unsigned char> data,
                       const blink::WebCryptoResult& result,
                       scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : BaseState(result, std::move(task_runner)),
        algorithm(algorithm),
        key(key),
        signature(std::move(signature)),
        data(std::move(data)),
        verify_result(false) {}

  const blink::WebCryptoAlgorithm algorithm;
  const blink::WebCryptoKey key;
  blink::WebVector<unsigned char> signature;
  blink::WebVector<unsigned char> data;

  bool verify_result;
};

struct WrapKeyState : public BaseState {
  WrapKeyState(blink::WebCryptoKeyFormat format,
               const blink::WebCryptoKey& key,
               const blink::WebCryptoKey& wrapping_key,
               const blink::WebCryptoAlgorithm& wrap_algorithm,
               const blink::WebCryptoResult& result,
               scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : BaseState(result, std::move(task_runner)),
        format(format),
        key(key),
        wrapping_key(wrapping_key),
        wrap_algorithm(wrap_algorithm) {}

  const blink::WebCryptoKeyFormat format;
  const blink::WebCryptoKey key;
  const blink::WebCryptoKey wrapping_key;
  const blink::WebCryptoAlgorithm wrap_algorithm;

  std::vector<uint8_t> buffer;
};

struct UnwrapKeyState : public BaseState {
  UnwrapKeyState(blink::WebCryptoKeyFormat format,
                 blink::WebVector<unsigned char> wrapped_key,
                 const blink::WebCryptoKey& wrapping_key,
                 const blink::WebCryptoAlgorithm& unwrap_algorithm,
                 const blink::WebCryptoAlgorithm& unwrapped_key_algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 const blink::WebCryptoResult& result,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : BaseState(result, std::move(task_runner)),
        format(format),
        wrapped_key(std::move(wrapped_key)),
        wrapping_key(wrapping_key),
        unwrap_algorithm(unwrap_algorithm),
        unwrapped_key_algorithm(unwrapped_key_algorithm),
        extractable(extractable),
        usages(usages) {}

  const blink::WebCryptoKeyFormat format;
  blink::WebVector<unsigned char> wrapped_key;
  const blink::WebCryptoKey wrapping_key;
  const blink::WebCryptoAlgorithm unwrap_algorithm;
  const blink::WebCryptoAlgorithm unwrapped_key_algorithm;
  const bool extractable;
  const blink::WebCryptoKeyUsageMask usages;

  blink::WebCryptoKey unwrapped_key;
};

struct DeriveBitsState : public BaseState {
  DeriveBitsState(const blink::WebCryptoAlgorithm& algorithm,
                  const blink::WebCryptoKey& base_key,
                  std::optional<unsigned int> length_bits,
                  const blink::WebCryptoResult& result,
                  scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : BaseState(result, std::move(task_runner)),
        algorithm(algorithm),
        base_key(base_key),
        length_bits(length_bits) {}

  const blink::WebCryptoAlgorithm algorithm;
  const blink::WebCryptoKey base_key;
  const std::optional<unsigned int> length_bits;

  std::vector<uint8_t> derived_bytes;
};

struct DeriveKeyState : public BaseState {
  DeriveKeyState(const blink::WebCryptoAlgorithm& algorithm,
                 const blink::WebCryptoKey& base_key,
                 const blink::WebCryptoAlgorithm& import_algorithm,
                 const blink::WebCryptoAlgorithm& key_length_algorithm,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usages,
                 const blink::WebCryptoResult& result,
                 scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : BaseState(result, std::move(task_runner)),
        algorithm(algorithm),
        base_key(base_key),
        import_algorithm(import_algorithm),
        key_length_algorithm(key_length_algorithm),
        extractable(extractable),
        usages(usages) {}

  const blink::WebCryptoAlgorithm algorithm;
  const blink::WebCryptoKey base_key;
  const blink::WebCryptoAlgorithm import_algorithm;
  const blink::WebCryptoAlgorithm key_length_algorithm;
  bool extractable;
  blink::WebCryptoKeyUsageMask usages;

  blink::WebCryptoKey derived_key;
};

// --------------------------------------------------------------------
// Wrapper functions
// --------------------------------------------------------------------
//
// * The methods named Do*() run on the crypto thread.
// * The methods named Do*Reply() run on the target Blink thread

void DoEncryptReply(std::unique_ptr<EncryptState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "DoEncryptReply");
  CompleteWithBufferOrError(state->status, state->buffer, &state->result);
}

void DoEncrypt(std::unique_ptr<EncryptState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoEncrypt");
  EncryptState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status = webcrypto::Encrypt(state->algorithm, state->key, state->data,
                                     &state->buffer);
  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoEncryptReply, std::move(passed_state)));
}

void DoDecryptReply(std::unique_ptr<DecryptState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "DoDecryptReply");
  CompleteWithBufferOrError(state->status, state->buffer, &state->result);
}

void DoDecrypt(std::unique_ptr<DecryptState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoDecrypt");
  DecryptState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status = webcrypto::Decrypt(state->algorithm, state->key, state->data,
                                     &state->buffer);
  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoDecryptReply, std::move(passed_state)));
}

void DoDigestReply(std::unique_ptr<DigestState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoDigestReply");
  CompleteWithBufferOrError(state->status, state->buffer, &state->result);
}

void DoDigest(std::unique_ptr<DigestState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoDigest");
  DigestState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::Digest(state->algorithm, state->data, &state->buffer);
  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoDigestReply, std::move(passed_state)));
}

void DoGenerateKeyReply(std::unique_ptr<GenerateKeyState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "DoGenerateKeyReply");
  if (state->status.IsError()) {
    CompleteWithError(state->status, &state->result);
  } else {
    state->generate_key_result.Complete(&state->result);
  }
}

void DoGenerateKey(std::unique_ptr<GenerateKeyState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoGenerateKey");
  GenerateKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::GenerateKey(state->algorithm, state->extractable,
                             state->usages, &state->generate_key_result);
  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoGenerateKeyReply, std::move(passed_state)));
}

void DoImportKeyReply(std::unique_ptr<ImportKeyState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "DoImportKeyReply");
  CompleteWithKeyOrError(state->status, state->key, &state->result);
}

void DoImportKey(std::unique_ptr<ImportKeyState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoImportKey");
  ImportKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::ImportKey(state->format, state->key_data, state->algorithm,
                           state->extractable, state->usages, &state->key);
  if (state->status.IsSuccess()) {
    DCHECK(state->key.Handle());
    DCHECK(!state->key.Algorithm().IsNull());
    DCHECK_EQ(state->extractable, state->key.Extractable());
  }

  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoImportKeyReply, std::move(passed_state)));
}

void DoExportKeyReply(std::unique_ptr<ExportKeyState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "DoExportKeyReply");
  if (state->format != blink::kWebCryptoKeyFormatJwk) {
    CompleteWithBufferOrError(state->status, state->buffer, &state->result);
    return;
  }

  if (state->status.IsError()) {
    CompleteWithError(state->status, &state->result);
  } else {
    state->result.CompleteWithJson(base::as_string_view(state->buffer));
  }
}

void DoExportKey(std::unique_ptr<ExportKeyState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoExportKey");
  ExportKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::ExportKey(state->format, state->key, &state->buffer);
  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoExportKeyReply, std::move(passed_state)));
}

void DoSignReply(std::unique_ptr<SignState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoSignReply");
  CompleteWithBufferOrError(state->status, state->buffer, &state->result);
}

void DoSign(std::unique_ptr<SignState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoSign");
  SignState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status = webcrypto::Sign(state->algorithm, state->key, state->data,
                                  &state->buffer);

  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoSignReply, std::move(passed_state)));
}

void DoVerifyReply(std::unique_ptr<VerifySignatureState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoVerifyReply");
  if (state->status.IsError()) {
    CompleteWithError(state->status, &state->result);
  } else {
    state->result.CompleteWithBoolean(state->verify_result);
  }
}

void DoVerify(std::unique_ptr<VerifySignatureState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoVerify");
  VerifySignatureState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::Verify(state->algorithm, state->key, state->signature,
                        state->data, &state->verify_result);

  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoVerifyReply, std::move(passed_state)));
}

void DoWrapKeyReply(std::unique_ptr<WrapKeyState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "DoWrapKeyReply");
  CompleteWithBufferOrError(state->status, state->buffer, &state->result);
}

void DoWrapKey(std::unique_ptr<WrapKeyState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoWrapKey");
  WrapKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::WrapKey(state->format, state->key, state->wrapping_key,
                         state->wrap_algorithm, &state->buffer);

  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoWrapKeyReply, std::move(passed_state)));
}

void DoUnwrapKeyReply(std::unique_ptr<UnwrapKeyState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "DoUnwrapKeyReply");
  CompleteWithKeyOrError(state->status, state->unwrapped_key, &state->result);
}

void DoUnwrapKey(std::unique_ptr<UnwrapKeyState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoUnwrapKey");
  UnwrapKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status = webcrypto::UnwrapKey(
      state->format, state->wrapped_key, state->wrapping_key,
      state->unwrap_algorithm, state->unwrapped_key_algorithm,
      state->extractable, state->usages, &state->unwrapped_key);

  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoUnwrapKeyReply, std::move(passed_state)));
}

void DoDeriveBitsReply(std::unique_ptr<DeriveBitsState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "DoDeriveBitsReply");
  if (!state->status.IsError()) {
    HistogramDeriveBitsTruncation(state->result.GetExecutionContext(),
                                  state->length_bits,
                                  state->status.warning_type());
  }
  CompleteWithBufferOrError(state->status, state->derived_bytes,
                            &state->result);
}

void DoDeriveBits(std::unique_ptr<DeriveBitsState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoDeriveBits");
  DeriveBitsState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status =
      webcrypto::DeriveBits(state->algorithm, state->base_key,
                            state->length_bits, &state->derived_bytes);
  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoDeriveBitsReply, std::move(passed_state)));
}

void DoDeriveKeyReply(std::unique_ptr<DeriveKeyState> state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
               "DoDeriveKeyReply");
  CompleteWithKeyOrError(state->status, state->derived_key, &state->result);
}

void DoDeriveKey(std::unique_ptr<DeriveKeyState> passed_state) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "DoDeriveKey");
  DeriveKeyState* state = passed_state.get();
  if (state->cancelled())
    return;
  state->status = webcrypto::DeriveKey(
      state->algorithm, state->base_key, state->import_algorithm,
      state->key_length_algorithm, state->extractable, state->usages,
      &state->derived_key);
  state->origin_thread->PostTask(
      FROM_HERE, base::BindOnce(DoDeriveKeyReply, std::move(passed_state)));
}

}  // namespace

WebCryptoImpl::WebCryptoImpl() {
}

WebCryptoImpl::~WebCryptoImpl() {
}

void WebCryptoImpl::Encrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    blink::WebVector<unsigned char> data,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!algorithm.IsNull());
  if (result.Cancelled())
    return;

  std::unique_ptr<EncryptState> state(new EncryptState(
      algorithm, key, std::move(data), result, std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(
          FROM_HERE, base::BindOnce(DoEncrypt, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::Decrypt(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    blink::WebVector<unsigned char> data,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!algorithm.IsNull());
  if (result.Cancelled())
    return;

  std::unique_ptr<DecryptState> state(new DecryptState(
      algorithm, key, std::move(data), result, std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(
          FROM_HERE, base::BindOnce(DoDecrypt, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::Digest(
    const blink::WebCryptoAlgorithm& algorithm,
    blink::WebVector<unsigned char> data,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!algorithm.IsNull());
  if (result.Cancelled())
    return;

  std::unique_ptr<DigestState> state(
      new DigestState(algorithm, blink::WebCryptoKey::CreateNull(),
                      std::move(data), result, std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::BindOnce(DoDigest, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::GenerateKey(
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(!algorithm.IsNull());
  if (result.Cancelled())
    return;

  std::unique_ptr<GenerateKeyState> state(new GenerateKeyState(
      algorithm, extractable, usages, result, std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(
          FROM_HERE, base::BindOnce(DoGenerateKey, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::ImportKey(
    blink::WebCryptoKeyFormat format,
    blink::WebVector<unsigned char> key_data,
    const blink::WebCryptoAlgorithm& algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (result.Cancelled())
    return;
  std::unique_ptr<ImportKeyState> state(
      new ImportKeyState(format, std::move(key_data), algorithm, extractable,
                         usages, result, std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(
          FROM_HERE, base::BindOnce(DoImportKey, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::ExportKey(
    blink::WebCryptoKeyFormat format,
    const blink::WebCryptoKey& key,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (result.Cancelled())
    return;
  std::unique_ptr<ExportKeyState> state(
      new ExportKeyState(format, key, result, std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(
          FROM_HERE, base::BindOnce(DoExportKey, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::Sign(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    blink::WebVector<unsigned char> data,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (result.Cancelled())
    return;
  std::unique_ptr<SignState> state(new SignState(
      algorithm, key, std::move(data), result, std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::BindOnce(DoSign, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::VerifySignature(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& key,
    blink::WebVector<unsigned char> signature,
    blink::WebVector<unsigned char> data,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (result.Cancelled())
    return;
  std::unique_ptr<VerifySignatureState> state(new VerifySignatureState(
      algorithm, key, std::move(signature), std::move(data), result,
      std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(FROM_HERE,
                                  base::BindOnce(DoVerify, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::WrapKey(
    blink::WebCryptoKeyFormat format,
    const blink::WebCryptoKey& key,
    const blink::WebCryptoKey& wrapping_key,
    const blink::WebCryptoAlgorithm& wrap_algorithm,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (result.Cancelled())
    return;
  std::unique_ptr<WrapKeyState> state(
      new WrapKeyState(format, key, wrapping_key, wrap_algorithm, result,
                       std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(
          FROM_HERE, base::BindOnce(DoWrapKey, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::UnwrapKey(
    blink::WebCryptoKeyFormat format,
    blink::WebVector<unsigned char> wrapped_key,
    const blink::WebCryptoKey& wrapping_key,
    const blink::WebCryptoAlgorithm& unwrap_algorithm,
    const blink::WebCryptoAlgorithm& unwrapped_key_algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (result.Cancelled())
    return;
  std::unique_ptr<UnwrapKeyState> state(
      new UnwrapKeyState(format, std::move(wrapped_key), wrapping_key,
                         unwrap_algorithm, unwrapped_key_algorithm, extractable,
                         usages, result, std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(
          FROM_HERE, base::BindOnce(DoUnwrapKey, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::DeriveBits(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& base_key,
    std::optional<unsigned int> length_bits,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (result.Cancelled())
    return;
  std::unique_ptr<DeriveBitsState> state(new DeriveBitsState(
      algorithm, base_key, length_bits, result, std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(
          FROM_HERE, base::BindOnce(DoDeriveBits, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

void WebCryptoImpl::DeriveKey(
    const blink::WebCryptoAlgorithm& algorithm,
    const blink::WebCryptoKey& base_key,
    const blink::WebCryptoAlgorithm& import_algorithm,
    const blink::WebCryptoAlgorithm& key_length_algorithm,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    blink::WebCryptoResult result,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  if (result.Cancelled())
    return;
  std::unique_ptr<DeriveKeyState> state(new DeriveKeyState(
      algorithm, base_key, import_algorithm, key_length_algorithm, extractable,
      usages, result, std::move(task_runner)));
  if (!CryptoThreadPool::PostTask(
          FROM_HERE, base::BindOnce(DoDeriveKey, std::move(state)))) {
    CompleteWithThreadPoolError(&result);
  }
}

bool WebCryptoImpl::DeserializeKeyForClone(
    const blink::WebCryptoKeyAlgorithm& algorithm,
    blink::WebCryptoKeyType type,
    bool extractable,
    blink::WebCryptoKeyUsageMask usages,
    const unsigned char* key_data,
    unsigned key_data_size,
    blink::WebCryptoKey& key) {
  return webcrypto::DeserializeKeyForClone(
      algorithm, type, extractable, usages,
      base::make_span(key_data, key_data_size), &key);
}

bool WebCryptoImpl::SerializeKeyForClone(
    const blink::WebCryptoKey& key,
    blink::WebVector<unsigned char>& key_data) {
  return webcrypto::SerializeKeyForClone(key, &key_data);
}

}  // namespace webcrypto
