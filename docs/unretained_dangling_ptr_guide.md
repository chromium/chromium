# Unretained dangling pointers.

*See also the main document about [dangling pointers](./dangling_ptr.md)*

**Background**
37.5% of Use-After-Free (UAF) vulnerabilities in Chrome stemmed from callbacks using dangling `base::Unretained` pointers.

**Mitigation:**
Runtime checks now help prevent callbacks with dangling pointers. A callback
invoked with dangling pointers is now turned into a crash.

**Note:** BackupRefPtr mitigates the security impact of accessing those dangling
pointers. Still, they are the source of crashes and indicate the memory
ownership is not tracked properly in Chrome. Don't rely solely on pointer values
without dereferencing, as addresses might be reused.

## Fixing the Unretained Dangling Pointer

1. **Ideal Solution:**  Refactor the code to ensure callbacks cannot be invoked
   after their argument is freed.
2. **WeakPtr<T>** If the argument's lifetime is not guaranteed to exceed the
   callback, the `WeakPtr<T>` is a good alternative, provided you can find a
   implement a reasonable fallback when the object is gone.
3. **Unique Identifiers:** Opt for unique identifiers (e.g., `base::IdType`)
   instead of pointers when the object is known to be freed. Unique identified
   are better than pointer, as addresses might be reused.

## Annotations

### `base::UnsafeDangling`

Opt-out of the check. Avoid if at all possible. Only for rare scenarios when you
are certain the pointer is dangling with no better alternatives.

When using `UnsafeDangling()`, the receiver must be of type
`MayBeDangling<>`. Example:

```cpp
void MyCallback(MayBeDangling<int> ptr);

int* may_be_dangling_ptr;
base::BindOnce(&MyCallback, base::UnsafeDangling(may_be_dangling_ptr));
```

### `base::UnsafeDanglingUntriaged`

Same as `UnsafeDangling()`, but with a different meaning:

- **Temporary Triage:** Used for flagging pre-existing issues needing migration
  to safer ownership. Chrome engineers still need to investigate the root cause.
  Those pointers might be unsafe.
- **Do NOT Use For New Code:** Only acceptable for release-impacting issues.
  Please prefer triaging pointers, as opposed to disabling the whole check
  globally.
