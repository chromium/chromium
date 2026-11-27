# Playbook for Reducing `.data.rel.ro`

This playbook catalogs techniques for moving large static tables and constants
out of `.data.rel.ro` into position-independent `.rodata`.

These patterns are optimization options, not general C++ style rules. Each
trades simplicity—and sometimes compile time or linker string merging—against
private dirty RAM and binary size. Apply them only when measurements identify a
large `.data.rel.ro` table worth the complexity (see
[When Not to Convert](#when-not-to-convert)), and weigh readability with code
reviewers.

Read [//docs/native_relocations.md](/docs/native_relocations.md) first for
background on how ELF/PE dynamic relocations work and why `.data.rel.ro` adds
startup latency and dirty RAM. See
[binary_size_explainer.md](binary_size_explainer.md) and
[optimization_advice.md](optimization_advice.md) for broader binary size
guidance.

[TOC]

## Measuring `.data.rel.ro` and Relocations

Measure before and after every conversion. Moving data from `.data.rel.ro` to
`.rodata` eliminates dynamic relocations, but total binary size can shrink or
grow depending on struct padding, lookup code size, cross-TU string
deduplication, and pointer width (4 bytes on 32-bit ARM vs. 8 bytes on 64-bit
platforms).

### Inspecting a Single Object File

Compile without ThinLTO (e.g., `use_thin_lto = false` or a standard Debug build)
so `.o` files contain ELF sections rather than LLVM bitcode:

```sh
autoninja -C out/Release obj/path/to/target/foo.o

# Show .data.rel.ro* sections, symbol sizes, and every relocation entry.
third_party/llvm-build/Release+Asserts/bin/llvm-readelf \
    --sections --symbols --relocs out/Release/obj/path/to/target/foo.o

# Distinguish .data from .data.rel.ro (also works on ThinLTO bitcode).
third_party/llvm-build/Release+Asserts/bin/llvm-nm \
    --format=darwin out/Release/obj/path/to/target/foo.o
```

### Inspecting Linked Binaries and Binary Size

```sh
# Total .data.rel.ro section sizes in a linked shared library (see
# //docs/native_relocations.md for full --relocs and Windows PE commands).
third_party/llvm-build/Release+Asserts/bin/llvm-readelf \
    --sections out/Release/libmonochrome.so | grep rel.ro

# Local SuperSize diff across .data.rel.ro, .rodata, and .text.
tools/binary_size/diagnose_bloat.py HEAD -v
```

Always check the `android-binary-size` trybot (see
[android_binary_size_trybot.md](android_binary_size_trybot.md)) on changes that
touch large tables.

## Techniques

### 1. Export `inline constexpr char[]` Instead of `extern`

As noted in [//docs/native_relocations.md](/docs/native_relocations.md), prefer
`char[]` over `const char* const`. Prefer `inline constexpr char[]` over
`extern const char[]` as well:

```cpp
// Header: zero relocations; characters are readable in consteval across TUs.
inline constexpr char kPrefName[] = "my.pref";
```

A `.cc` file cannot inspect the characters of an `extern const char[]` constant
at compile time, so listing `extern` pref or switch names in a static table
forces a dynamic relocation per entry. ThinLTO does not eliminate those
relocations for runtime-indexed tables because `consteval` runs in the frontend
before IR generation and ThinLTO does not rewrite pointer arrays into
integer-offset pools. Switching those constants to `inline constexpr char[]`
lets downstream tables copy or pool them at compile time.

### 2. Inline Fixed-Width Character Arrays

When every string has the same length (2-char country codes, 4-char ISO 15924
script or brand codes, 32-char CRX IDs) or a short bounded maximum (typically
`<= 12` bytes), store the characters inline in the struct.

For C-string consumers, inline `char[N]` arrays decay to `const char*` at call
sites with minimal churn:

```cpp
// Before: 2 pointers per entry in .data.rel.ro (16 bytes on 64-bit).
struct LegacyEncoding {
  const char* domain;
  const char* encoding;
};

// After: 16 bytes per entry in .rodata, 0 relocations.
struct LegacyEncoding {
  char domain[3];     // 2 chars + NUL
  char encoding[13];  // up to 12 chars + NUL
};
```

For `base::MakeFixedFlatSet`, `base::MakeFixedFlatMap`, or sorted arrays, wrap
`std::array<char, N>` in a small `constexpr` key type (see
`autofill::FixedCountryCode` in
[//components/autofill/core/browser/geo/country_data.h](/components/autofill/core/browser/geo/country_data.h)
for an in-tree `std::array<char, 2>` wrapper). Adding heterogeneous
`std::string_view` comparisons avoids constructing a temporary key on lookup:

```cpp
template <size_t N>
struct FixedBrandCode {
  std::array<char, N> chars{};

  // NOLINTNEXTLINE(google-explicit-constructor)
  consteval FixedBrandCode(const char (&s)[N + 1]) {
    auto input = base::span(s);
    CHECK(input.back() == '\0');
    base::span(chars).copy_from(input.template first<N>());
    CHECK(!std::string_view(chars).contains('\0'));
  }

  constexpr std::string_view view() const LIFETIME_BOUND {
    return std::string_view(chars);
  }

  // Compare via string_view so byte order is unsigned on every architecture.
  friend constexpr bool operator==(const FixedBrandCode& a,
                                   const FixedBrandCode& b) {
    return a.view() == b.view();
  }
  friend constexpr auto operator<=>(const FixedBrandCode& a,
                                    const FixedBrandCode& b) {
    return a.view() <=> b.view();
  }
  friend constexpr bool operator==(const FixedBrandCode& a,
                                   std::string_view b) {
    return a.view() == b;
  }
  friend constexpr auto operator<=>(const FixedBrandCode& a,
                                    std::string_view b) {
    return a.view() <=> b;
  }
};
```

`base::MakeFixedFlatSet` and `base::MakeFixedFlatMap` default to `std::less<>`,
so heterogeneous lookups with `std::string_view` work directly. For
`std::ranges::binary_search` on a `std::array<FixedBrandCode<N>, M>`, pass a
projection
(`std::ranges::binary_search(kCodes, sv, {}, &FixedBrandCode<N>::view)`) or
`std::less<>{}`, because `std::ranges::less` requires a common reference type
between the element and search key.

**Size check:** Compare wasted tail padding `entries * (max_len - avg_len)`
against pointer savings `entries * sizeof(void*)` (4 bytes on 32-bit ARM). When
the average length is much smaller than the maximum, use a string pool instead.

### 3. Flat String Pools with Integer Offsets

For variable-length strings, pack all characters into a single contiguous
`constexpr` array and reference entries by integer offset (`uint16_t` when the
pool fits in 64 KiB, `uint32_t` otherwise).

#### Existing `//base` Helpers

Chromium provides two zero-relocation types in `//base` (intended primarily for
generated tables):

*   `base::subtle::StringSlice<N, kData>`
    ([//base/strings/string_slice.h](/base/strings/string_slice.h)): stores
    `{offset, length}` using the smallest unsigned integer that holds `N`
    (`uint8_t`, `uint16_t`, or `uint32_t`) and converts implicitly to
    `std::string_view`. Best when strings do not need NUL terminators and
    callers want `std::string_view` in O(1) time.
*   `base::subtle::IndexPointer<T, base>`
    ([//base/memory/index_pointer.h](/base/memory/index_pointer.h)): stores an
    `unsigned int` offset from a static array and converts to `const T*`. Best
    when callers expect NUL-terminated `const char*` pointers (or pointers into
    an element pool).

#### Hand-Written `consteval` Two-Pass Packing

For checked-in C++ tables or X-macro `.inc` files, you can pack the pool at
compile time in two `consteval` passes: pass 1 computes the exact pool size, and
pass 2 fills `std::array<char, kStringPoolSize>` plus an offset array.

Keep the raw pointer table in an anonymous namespace (never with external
linkage) and reference it only from `consteval` lambdas or functions so the
compiler does not emit it into `.data.rel.ro` (verify with `llvm-readelf` that
no runtime ODR-use accidentally retains it). The example below packs entries
sequentially with a trailing sentinel offset (`offsets[N] == kStringPoolSize`);
see below for NUL-terminated or deduplicated variants:

```cpp
namespace {

struct RawEntry {
  uint32_t value;
  std::string_view name;
};

constexpr auto kRawEntries = std::to_array<RawEntry>({
    {0x0001, "ALPHA"},
    {0x0002, "BETA_LONG_NAME"},
});
constexpr size_t kEntryCount = kRawEntries.size();

constexpr size_t kStringPoolSize = []() consteval {
  size_t total = 0;
  for (const auto& entry : kRawEntries) {
    total += entry.name.size();
  }
  return total;
}();
static_assert(kStringPoolSize <= std::numeric_limits<uint16_t>::max());

struct PackedTable {
  std::array<uint32_t, kEntryCount> values{};
  std::array<uint16_t, kEntryCount + 1> name_offsets{};
  std::array<char, kStringPoolSize> string_pool{};
};

constexpr PackedTable kTable = []() consteval {
  PackedTable out;
  size_t pos = 0;
  for (size_t i = 0; i < kEntryCount; ++i) {
    out.values[i] = kRawEntries[i].value;
    out.name_offsets[i] = static_cast<uint16_t>(pos);
    for (char c : kRawEntries[i].name) {
      out.string_pool[pos++] = c;
    }
  }
  out.name_offsets[kEntryCount] = static_cast<uint16_t>(pos);
  return out;
}();

std::string_view GetEntryName(size_t index) {
  size_t start = kTable.name_offsets[index];
  size_t length = kTable.name_offsets[index + 1] - start;
  return std::string_view(kTable.string_pool).substr(start, length);
}

}  // namespace
```

**Trade-offs:** Two-pass `consteval` walks `kRawEntries` twice at compile time
(because `std::array` requires `kStringPoolSize` as a template argument), adding
compile time on very large tables. Because RELR already compresses relative
relocation metadata tightly on disk, a private pool mainly wins by eliminating
dirty `.data.rel.ro` pages and shrinking 64-bit pointers to
`uint16_t`/`uint32_t` offsets—and can grow binary size by disabling cross-TU
string merging (see [Pitfalls](#pitfalls)).

#### Types of String Pools

There are 3 common options for string pools:

1)  Store `offset` & `length` explicitly.
    * Best when there is string duplication.
2)  Store only `offsets` and use `offsets[i + 1] - offset[i]s` to compute the
    length.
    * Best when there is no duplication.
3)  Use null-terminated strings and rely on `strlen`
    * Best when you need null-terminated strings.

### 4. Build-Time Code Generators

Consider a build-time generator when a table already comes from JSON, IDL, GRD,
or text inputs, or when packing it in C++ (for example, >~1,000 entries or
nested arrays) would require complicated `consteval` or macro logic, slow
compile times, or hit Clang's `-fconstexpr-steps` limit. Generators add
build-graph indirection and maintenance overhead, so avoid them for small or
straightforward tables; when C++ packing would otherwise be intricate, a
generator's main readability advantage is that the generated C++ source is
concrete and easy to inspect directly instead of tracing `consteval` or macro
expansions.

#### Packing Nested Spans (`base::span<const T>`)

Static structs containing `base::span<const T>` hold a relocatable pointer plus
a length. Replace nested sub-arrays with a single flat pool and `(offset, size)`
slices:

```cpp
// Generated in .rodata with 0 relocations.
inline constexpr char kStringPool[] = "\0" "foo\0" "bar\0";
inline constexpr uint32_t kItemPool[] = {10, 20, 30, 40};

struct Slice {
  uint16_t offset;
  uint16_t size;
};

struct Entry {
  uint16_t name_offset;  // index into kStringPool
  Slice items;           // subspan of kItemPool
};

inline constexpr Entry kEntries[] = {
    {1, {0, 2}},  // "foo", {10, 20}
    {5, {2, 2}},  // "bar", {30, 40}
};
```

#### Generator Guidelines

*   **Sub-slice deduplication:** Just as with string pools, deduplicating
    identical sub-arrays in `kItemPool` requires explicit `(offset, size)`
    slices, whereas non-deduplicated monotonic sub-arrays can use `N + 1`
    sentinel offsets.
*   **UTF-16 code units in Python:** When emitting `char16_t` (`u"..."`) pools,
    compute offsets with `len(s.encode('utf-16-le')) // 2`, not `len(s)`.
    Non-BMP characters occupy 2 UTF-16 code units, and `'utf-16-le'` omits the
    BOM.
*   **Fail the build on overflow:** Check `offset > 0xFFFF` (or `size > 0xFFFF`)
    in the generator and raise an error rather than silently truncating in C++.
*   **Keep checked-in helpers out of Python:** Put non-codegen-dependent struct
    definitions and lookup logic into checked-in `.h`/`.cc` files, and generate
    only the data tables.
*   **Avoid threading pool pointers through sub-struct methods:** Put lookup
    methods on the top-level `Data` holder (`data.Contains(entry, ...)`) instead
    of adding a `const Data&` parameter to every nested struct method
    (`entry.Contains(data, ...)`). That keeps table structs pure POD.
*   **Split new generators into two stacked CLs:** When introducing a new Python
    generator for a large hand-written table, land CL 1 emitting byte-identical
    symbols, then stack CL 2 to switch the generated representation to `.rodata`
    pools.

### 5. Struct Packing, SoA Striping, and Isolating Pointers

Eliminating pointers from a struct is only half the win; poor alignment can
waste `.rodata` bytes or leave unrelated fields in `.data.rel.ro`.

*   **Order fields by descending alignment:** Place `uint64_t`/`uint32_t` before
    `uint16_t` before `uint8_t`/`bool`, and add `static_assert(sizeof(Entry) ==
    ...)` to prevent accidental padding regressions.
*   **Stripe mismatched alignments (Struct-of-Arrays):** Replacing `const char*`
    (8 bytes) with `uint16_t` (2 bytes) inside `struct { uint32_t value;
    uint16_t offset; }` still takes 8 bytes per entry due to 2 bytes of tail
    padding. Splitting into parallel arrays (`std::array<uint32_t, N> kValues`
    and `std::array<uint16_t, N + 1> kNameOffsets`) eliminates the padding,
    pairs directly with sentinel offsets, and improves cache locality when
    lookups scan or binary-search a single column without pulling cold fields
    into cache lines.
*   **Separate unavoidable pointers from pure data:** A single remaining pointer
    field (such as a function pointer, `const base::Feature*`, or cross-TU
    `extern` pointer) forces the *entire* array of structs into `.data.rel.ro`.
    If each 64-byte entry has one 8-byte pointer and 56 bytes of strings/flags,
    split the pointers into a parallel `kPointers[N]` array so the other 87.5%
    of each record moves to clean `.rodata`.

### 6. Intra-Table Indices and Sorted `.rodata` Lookup Tables

*   **Replace intra-table pointers with indices:** A static lookup table storing
    `const Entry*` pointers into a sibling array in the same file creates one
    relocation per entry. Store a `uint8_t` or `uint16_t` index instead.
*   **Replace `MakeFixedFlatMap<std::string_view, V>` with a sorted array:**
    `base::MakeFixedFlatMap` and `base::MakeFixedFlatSet` keyed by
    `std::string_view` store a pointer per entry. Store a sorted
    `std::array<CompactEntry, N>` with `uint16_t` string-pool offsets and search
    it with `std::ranges::lower_bound` or `std::ranges::binary_search`:

    ```cpp
    auto it = std::ranges::lower_bound(kEntries, key, {}, &CompactEntry::name);
    if (it != kEntries.end() && it->name() == key) {
      return it->value;
    }
    ```

    Ensure the array is sorted in unsigned byte (UTF-8 lexicographical) order to
    match `std::string_view` comparison (Python's `sorted()` on `str` matches
    UTF-8 byte order). Add a `static_assert(std::ranges::is_sorted(...))` for
    hand-maintained tables.

*   **Eliminate hidden `.L.ref.tmp` arrays:** Initializing a
    `base::NoDestructor<std::unordered_map<std::string_view, V>>` (or
    `absl::flat_hash_map`) from a brace-enclosed `std::initializer_list` emits a
    hidden `.L.ref.tmp.*` array of string pointers in `.data.rel.ro` *and*
    allocates the map on the heap at runtime. Replacing it with a sorted
    `.rodata` `std::array` removes both the relocations and the heap allocation.

### 7. Specialized Encodings

#### `gperf` Tables: Use `%pic`

By default, `gperf` emits a word table containing a `const char*` per slot. Add
`%pic` to the `.gperf` input (see
[//third_party/blink/renderer/build/scripts/core/css/templates/css_value_keywords.cc.tmpl](/third_party/blink/renderer/build/scripts/core/css/templates/css_value_keywords.cc.tmpl))
so `gperf` emits a single string pool and integer offsets in `.rodata`.

#### Large String Sets: DAFSA

For large static sets of printable 7-bit ASCII strings (domain lists, TLD lists,
expired flag lists) or mappings from ASCII strings to small integers (`0–7`),
use [//net/tools/dafsa/make_dafsa.py](/net/tools/dafsa/make_dafsa.py) with
`net::LookupStringInFixedSet`
([//net/base/lookup_string_in_fixed_set.h](/net/base/lookup_string_in_fixed_set.h)).
A DAFSA compiles the strings into a compact `.rodata` byte graph with aggressive
prefix and suffix sharing and zero relocations.

#### Sequential-Only Tables and Procedural Registration

If a table is only walked linearly once (for example, registering preferences or
event handler names), you do not need an offset array:

*   Concatenate string literals with explicit `"\0"` separators (e.g., an
    X-macro expanding `X(foo)` to `"foo\0"`) into a single `constexpr char
    kNames[]` and iterate by advancing `sv.size() + 1`.
*   If a table expands a small Cartesian product (such as
    `"webkit.webprefs.fonts.<family>.<script>"`) solely to register keys into a
    runtime map that copies them into `std::string`, store only the short factor
    arrays in `.rodata` and format each key into a stack buffer in a loop.

#### Hash-Only Tables

When callers only test membership or map a string key to a small value and never
enumerate or print the original keys, consider storing sorted `uint64_t` (or
build-verified collision-free `uint32_t`) hashes in `.rodata` and dropping the
string pool entirely. Verify zero collisions at build time, and only use
truncated hashes where the collision probability against untrusted inputs is
acceptable.

### 8. Compiler-Generated Relocation Tables

Relocations do not only come from explicit global arrays; the compiler can also
synthesize `.data.rel.ro` tables for control flow:

*   **Dense `switch` returning pointers or `std::string_view`:** Clang lowers
    dense `switch (enum_val)` statements returning string literals or global
    object references into a `[Switch table]` pointer array in `.data.rel.ro`.
    Replace the `switch` with direct indexing into a `.rodata` string-offset
    array (or a contiguous storage array ordered by enum value).
*   **Multi-variant `std::visit`:** Calling `std::visit(visitor, v1, v2)` on a
    `std::variant` with `N` alternatives emits an `N × N` function-pointer
    dispatch matrix (`__fmatrix`) in `.data.rel.ro`. When off-diagonal
    combinations return a constant default, guard with `if (v1.index() !=
    v2.index()) { return false; }` and dispatch on a single variant—shrinking
    `__fmatrix` from `N × N` to `N` (or use an explicit `switch` on `v1.index()`
    to eliminate `__fmatrix` completely).

## Pitfalls

*   **Linker string merging (`SHF_MERGE | SHF_STRINGS`):** Normal C++ string
    literals live in `.rodata.str1.1` with `SHF_MERGE | SHF_STRINGS`, allowing
    the linker to deduplicate identical strings across the entire binary.
    Strings packed into a custom `constexpr char kStringPool[]` are *not* merged
    across translation units. If a table duplicates strings already present
    elsewhere (such as feature or flag names)—or is small on 32-bit builds where
    pointers are only 4 bytes—a private pool can grow binary size even while
    dropping `.data.rel.ro` to zero.
*   **Dangling `std::string_view` returns:** Annotate view getters backed by
    inline struct storage with `LIFETIME_BOUND`, and never return a
    `std::string_view` referencing a temporary or a mutable cache entry.
*   **Component builds (`is_component_build = true`):** Moving helper methods
    between structs or removing `*_EXPORT` annotations can break component-build
    linking (`out/Debug`) even when static Release builds pass. Always verify a
    component build.
*   **`-Werror,-Wunreachable-code-return` in `consteval` checks:** Inside a
    `consteval` validation lambda, checking a trivially constant expression
    (such as `if (kOffsets.front() != 0) { return false; }`) can fail bots with
    `-Wunreachable-code-return`. Hoist constant invariants into standalone
    `static_assert`s.

## When Not to Convert

Skip conversion when the cost outweighs the relocation savings:

*   **Public third-party or C ABI structs:** Tables whose elements are returned
    by raw pointer across a fixed C API (such as FFmpeg codec descriptors or
    Android `JNINativeMethod` registration arrays).
*   **Polymorphic singleton arrays:** Arrays of objects carrying a `vptr` where
    virtual dispatch is central to the design.
*   **Tiny tables:** Tables with fewer than ~10–20 relocations where lookup
    `.text` or readability cost outweighs a few dozen bytes of `.data.rel.ro`.
