# Dangling Pointer Detector

A pointer is dangling when it references freed memory. Typical examples can be
found [here](https://docs.google.com/document/d/11YYsyPF9rQv_QFf982Khie3YuNPXV0NdhzJPojpZfco/edit?resourcekey=0-h1dr1uDzZGU7YWHth5TRAQ#heading=h.wxt96wl0k0sq).

Dangling pointers are not a problem unless they are subsequently dereferenced
and/or used for other purposes. Proving that pointers are unused has turned out
to be difficult in general, especially in face of future modifications to
the code. Hence, they are a source of UaF bugs and highly discouraged unless
you are able to ensure that they can never be used after the pointed-to objects
are freed.

See also the [Dangling Pointers Guide](./dangling_ptr_guide.md) for how to fix
cases where dangling pointers occur.

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

The `DanglingUntriaged` option has been used to annotate pre-existing dangling
pointers in Chrome:
```cpp
raw_ptr<T, DanglingUntriaged> ptr_dangling_mysteriously;
```
Contrary to `DisableDanglingPtrDetection`, we don't know yet why it dangles. It
is meant to be either refactored to avoid dangling, or turned into
"DisableDanglingPtrDetection" with a comment explaining what happens.

# How to check for dangling pointers?

On **Linux**, it is **enabled by default** on most configurations.
To be precise: (`is_debug` or `dcheck_always_on`) and non `is_official` builds.

For the other operating systems, this is gated by both build and runtime flags:

## Build flags

```bash
gn args ./out/dangling/
```

```gn
use_remoteexec = true
is_debug = false  # Important! (*)
is_component_build = false  # Important! (*)
dcheck_always_on = true
enable_backup_ref_ptr_support = true  # true by default on some platforms
enable_dangling_raw_ptr_checks = true
```

(*) We want to emphasize that setting either `is_debug = false` or
`is_component_build = false` is important. It is a common mistake to set
`is_debug` to `true`, which in turn turns on component builds, which
disables PartitionAlloc-Everywhere. `enable_backup_ref_ptr_support = true` can't
be used without PartitionAlloc-Everywhere, and is silently set to `false`.

## Runtime flags

```bash
./out/dangling/content_shell \
   --enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtr
```

By default, Chrome will crash on the first dangling raw_ptr detected.

# Runtime flags options:

## Mode parameter

### Crash (default)

```bash
--enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtr:mode/crash
```

### Record a list of signatures

Example usage:
```bash
./out/dangling/content_shell \
   --enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtr:mode/log_only \
   |& tee output
```

The logs can be filtered and transformed into a tab separated table:
```bash
cat output \
 | grep "[DanglingSignature]" \
 | cut -f2,3,4,5 \
 | sort \
 | uniq -c \
 | sed -E 's/^ *//; s/ /\t/' \
 | sort -rn
```

This is used to list issues and track progresses.

## Type parameter
### Select all dangling raw_ptr (default)

The option: `type/all` selects every dangling pointer.

Example usage:
```bash
./out/dangling/content_shell \
   --enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtr:type/all
```

### Select cross tasks dangling raw_ptr

The option: `type/cross_task` selects dangling pointers that are released in a
different task than the one where the memory was freed. Those are more likely to
cause UAF.

Example usage:
```bash
./out/dangling/content_shell \
   --enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtr:type/cross_task
```

### Combination

Both parameters can be combined, example usage:
```bash
./out/dangling/content_shell \
   --enable-features=PartitionAllocBackupRefPtr,PartitionAllocDanglingPtr:mode/log_only/type/cross_task \
   |& tee output
```

# Alternative dangling pointer detector (experimental)

The dangling pointer detector above works only against certain heap allocated
objects, but there is an alternate form that catches other cases such as
pointers to out-of-scope stack variables or pointers to deallocated shared
memory regions. The GN arguments to enable it are:

```gn
enable_backup_ref_ptr_support=false
is_asan=true
is_component_build=false
use_asan_backup_ref_ptr=false
use_raw_ptr_asan_unowned_impl=true
```

This will crash when the object containing the dangling ptr is destructed,
giving the usual three-stack trace from ASAN showing where the deleted object
was allocated and freed.

When running under this mode, there is no need to specify any --enable-features
flag as above.
