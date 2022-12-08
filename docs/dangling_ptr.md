# Dangling pointer detector.

Dangling pointers are not a problem unless they are dereferenced and used.
However, they are a source of UaF bugs and highly discouraged unless you are
100% confident that they are never dereferenced after the pointed-to objects are
freed.

See also the guide: [how to fix dangling pointers.
[docs/dangling_ptr.md](./dangling_ptr_guide.md)

Behind build flags, Chrome implements a dangling pointer detector. It causes
Chrome to crash, whenever a raw_ptr becomes dangling:
```cpp
raw_ptr<T> ptr_never_dangling;
```

On the other hand, we cannot simply ban all the usage of dangling pointers
because there are valid use cases. The `DisableDanglingPtrDetection` option can
be used to annotate "intentional-and-safe" dangling pointers. It is meant to be
used as a last resort, only if there is no better way to re-architecture the
code.
```cpp
raw_ptr<T, DisableDanglingPtrDetection> ptr_may_dangle;
```

The `DanglingUntriaged` has been used to annotate pre-existing dangling
pointers in Chrome:
```cpp
raw_ptr<T, DanglingUntriaged> ptr_dangling_mysteriously;
```
Contrary to `DisableDanglingPtrDetection`, we don't know yet why it dangles. It
is meant to be either refactored to avoid dangling, or turned into
"DisableDanglingPtrDetection" with a comment explaining what happens.

# How to check for dangling pointers?

It is gated behind both build and runtime flags:

## Build flags

```bash
gn args ./out/dangling/
```

```gn
use_goma = true
is_debug = false  # Important! (*)
dcheck_always_on = true
enable_backup_ref_ptr_support = true  # true by default on most platforms
enable_dangling_raw_ptr_checks = true
```

(*) We want to emphasize that `is_debug = false` is important. It is a common
mistake to set it to `true`, which in turn turns on component builds, which
disables PartitionAlloc-Everywhere. `enable_backup_ref_ptr_support = true` can't
be used without PartitionAlloc-Everywhere, and is silently set to `false`.

## Runtime flags

```bash
./out/dangling/content_shell \
   --enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtr
```

By default, Chrome will crash on the first dangling raw_ptr detected.

# Runtime flags options:

### Crash (default)

```bash
--enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtr:mode/crash
```

### Record a list of signatures

Example usage:
```bash
./out/dangling/content_shell \
   --enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtr:mode/log_signature \
   |& tee output
```

The logs can be filtered and transformed into a tab separated table:
```bash
cat output \
 | grep "DanglingSignature" \
 | cut -f2,3 \
 | sort \
 | uniq -c \
 | sed -E 's/^ *//; s/ /\t/' \
 | sort -rn
```

This is used to list issues and track progresses.
