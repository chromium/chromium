# Danling pointer detector.

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
use_backup_ref_ptr = true
use_partition_alloc = true
enable_dangling_raw_ptr_checks = true
```

## Runtime flags

```bash
./out/dangling/content_shell --enable-features=PartitionAllocBackupRefPtr
```

Chrome will crash on the first dangling raw_ptr detected.

# Record a list of signatures.

Instead of immediately crashing, you can list all the dangling raw_ptr
occurrences. This is gated behind the `PartitionAllocDanglingPtrRecord` feature.

For instance:
```bash
./out/dangling/content_shell --enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtrRecord |& tee output
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
