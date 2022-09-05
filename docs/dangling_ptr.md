# Dangling pointer detector.

Dangling pointers are not a problem unless they are dereferenced and used.
However, they are a source of UaF bugs and highly discouraged unless you are
100% confident that they are never dereferenced after the pointed-to objects are
freed.

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

# How to check for dangling pointers?

It is gated behind both build and runtime flags:

## Build flags

```bash
gn args ./out/dangling/
```

```gn
use_goma = true
is_debug = false
dcheck_always_on = true
enable_backup_ref_ptr_support = true
enable_dangling_raw_ptr_checks = true
use_backup_ref_ptr = true
```

We want to emphasize that `is_debug = false` is important. It is a common
mistake to set it to true, which in turn turns on component builds, which
disabled PartitionAlloc-Everywhere. `use_backup_ref_ptr = true` can't be used
without PartitionAlloc-Everywhere, leading to error:
```
ERROR at //base/allocator/allocator.gni:126:3: Assertion failed.
  assert(!use_backup_ref_ptr || use_allocator == "partition",
```

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

# DanglingUntriaged

This raw_ptr option means it is allowed to dangle. Contrary to
`DisableDanglingPtrDetection`, we don't know yet why it dangle. It is meant to
be either refactored to avoid dangling, or turned into
"DisableDanglingPtrDetection" with a comment explaining what happens.
