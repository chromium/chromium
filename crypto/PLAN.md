# //crypto plan

This doc outlines what the current maintainers of //crypto are doing with this
directory and why. If you feel like pitching in with any of these tasks, please
do. All of them are happening across the entire //crypto directory, tracked by
bugs in [Chromium > Internals > Crypto](https://issues.chromium.org/issues?q=status:open%20componentid:1768937&s=created_time:desc)
and by the overall [tend to //crypto bug](https://issues.chromium.org/issues/367888389).

If you have questions that aren't answered here, or would just like to help out,
visit [#security in slack](https://app.slack.com/client/T039UTRBS/CGK7KLZD4).

## Use Spans & Arrays When Possible

Convert functions that use (pointer, length) pairs to functions that use spans.
Also, favor return types that indicate that the contained data is bytes rather
than text, use fixed-size types when possible, and favor returning values rather
than using out parameters.

In general, crypto functions accept and return bytes, so for inputs:

* `base::span<const uint8_t, N>` for fixed-size inputs
* `base::span<const uint8_t>` for variably-sized inputs
* `std::string_view` overloads if there are many callers that need to pass text
  in directly

And for outputs:

* Returning `std::array<uint8_t, N>` for fixed-size outputs
* Returning `std::vector<uint8_t>` for variably-sized outputs

If an out parameter is needed, `std::span<uint8_t, N>` or `std::span<uint8_t>`
should be used, but generally you can avoid these.

If a function can fail (eg, decryption for an AEAD) but also needs to return a
value, it should return a `std::optional<std::array<uint8_t, N>>` or `
std::optional<std::vector<uint8_t>>` so that callers can't fail to check the
result. Don't return a success/failure bool and use an out parameter.

In all cases, never use (pointer, length) in //crypto APIs - there are always
better choices.

## Reduce Heap Allocations

As a general rule, let callers pass in spans of raw bytes, and return
std::arrays when returning new values, which gives the caller the most control
over allocation behavior. Let callers that need long-lived copies of return
values make those copies themselves.

Also, avoid factory functions that return `unique_ptr<T>` unless you really need
the caller to have a separate heap-allocated object - prefer factory functions
that return a `T` directly, or a `std::optional<T>` if creation has to be
fallible.

## Prefer Statelessness

For API design, prefer a free function over an object - don't have an object
unless the operations being done actually are stateful (eg, a streaming hash /
encryption API) or have expensive setup work that shouldn't be repeated (eg,
parsing an ASN.1 structure). Also, prefer objects that just hold state and are
used by free functions over adding methods to those objects.

For example, instead of:

```
class RSAEncryptor {
 public:
  RSAEncryptor();
  bool ImportKey(base::span<const uint8_t> key);
  void GenerateKey(size_t length);

  std::vector<uint8_t> EncryptPKCS1v15(base::span<const uint8_t> message);
  std::vector<uint8_t> EncryptOAEP(base::span<const uint8_t> message, ...);
 private:
  bssl::ScopedEVP_PKEY key_;
};
```

prefer something like:

```
class RSAKey {
 public:
  // Note that these are both static, so these are both also basically free
  // functions.
  static RSAKey Generate(size_t length);
  static std::optional<RSAKey> Import(base::span<const uint8_t> key);
 private:
  RSAKey();
  bssl::ScopedEVP_PKEY key_;
};

std::vector<uint8_t> EncryptPKCS1v15(const RSAKey& key,
                                     base::span<const uint8_t> message);
std::vector<uint8_t> EncryptOAEP(const RSAKey& key,
                                 base::span<const uint8_t> message,
                                 ...);
```

Since places that need to handle cryptographic state are far more common than
places that need to do cryptographic operations, it's helpful to separate the
state from the operations like this. In some cases, the operations are actually
in a separate header file (eg [crypto/keypair.h](keypair.h) vs
[crypto/sign.h](sign.h).

## Don't Handle OOM Conditions

Older //crypto APIs often tolerate `nullptr` returns from BoringSSL, even in
situations where these can only happen as a result of lack of memory. To
tolerate that, the //crypto APIs were made "fallible" in that they themselves
could return `nullptr` or `false` or otherwise fail to do what they were
supposed to.

In production chromium code, BoringSSL can never return an OOM failure: OOMs
cause an orderly runtime crash in the memory allocator. That means that any OOM
return is a can't-happen case in //crypto, and we should `CHECK()` that they
never happen (in case `nullptr` gets returned from some other reason, which
might indicate a bug on our part).

That means that old code which looks like this:

```
  bssl::UniquePtr<RSA> rsa_key(RSA_new());
  bssl::UniquePtr<BIGNUM> bn(BN_new());
  if (!rsa_key.get() || !bn.get() || !BN_set_word(bn.get(), 65537L))
    return nullptr;
```

would instead just be:

```
  bssl::UniquePtr<RSA> rsa_key(RSA_new());
  bssl::UniquePtr<BIGNUM> bn(BN_new());

  CHECK(rsa_key.get());
  CHECK(bn.get());
  CHECK(BN_set_word(bn.get(), 65537L));
```

with the result that, from the caller's perspective, generating an RSA key now
cannot fail. That simplifies client code a lot.

## Don't Support Obsolete Crypto

In general, //crypto should not expose broken or obsolete mechanisms to callers.
Unfortunately Chromium is sometimes required to use these mechanisms anyway for
compatibility; when we do have to add them, prefer to put them in
//crypto/obsolete with explicit restrictions on who can use them. See for
example [crypto/obsolete/md5.h](obsolete/md5.h).

Right now we consider these mechanisms obsolete, but they are still used in some
places in the codebase:

* MD5
* SHA-1
* RSA with key lengths less than 2048

## Be Explicit About What Primitives Are Used

Don't build an API like:

```
std::vector<uint8_t> SecureHash(base::span<const uint8_t> input);
```

This might seem appealing at first because it looks like it might allow us to
upgrade the hash function callers are using without changing call sites, but in
practice, you will have to implement it with a specific underlying hash
function. Once you do that, callers who actually do require *specifically that
hash function* will begin calling your wrapper function instead, thus forcing
you never to switch hash functions anyway. At the //crypto layer, it's better to
instead just expose:

```
std::array<uint8_t, ...> Sha256(base::span<const uint8_t> input);
std::array<uint8_t, ...> Sha512(base::span<const uint8_t> input);
```

... and so on, and force callers to explicitly specify what they want. Leave
opinionated wrappers like the hypothetical `SecureHash` to higher layers.

## Write Unit Tests

Some of the older //crypto code has few or no tests. When you're adding a new
API, try to write thorough test cases for it. Using existing known-answer tests
is best - often the relevant spec will include these, and you can be very
confident that they are correct.

If your API is too difficult to unit-test, it is probably poorly designed and
you should rethink it.

Also, you can reasonably assume the underlying BoringSSL primitive works
correctly, so it's not necessary to write tests asserting that BoringSSL itself
rejects invalid inputs / etc (although it is fine to write tests for the failure
behavior of //crypto APIs). In general one or two known-answer tests are
sufficient to ensure that data is being passed into and out of BoringSSL
correctly.
