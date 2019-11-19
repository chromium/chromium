"""Abseil compiler options.

This is the source of truth for Abseil compiler options.  To modify Abseil
compilation options:

  (1) Edit the appropriate list in this file based on the platform the flag is
      needed on.
  (2) Run `<path_to_absl>/copts/generate_copts.py`.

The generated copts are consumed by configure_copts.bzl and
AbseilConfigureCopts.cmake.
"""

# /Wall with msvc includes unhelpful warnings such as C4711, C4710, ...
MSVC_BIG_WARNING_FLAGS = [
    "/W3",
]

LLVM_BIG_WARNING_FLAGS = [
    "-Wall",
    "-Wextra",
    "-Weverything",
]

# Docs on single flags is preceded by a comment.
# Docs on groups of flags is preceded by ###.
LLVM_DISABLE_WARNINGS_FLAGS = [
    # Abseil does not support C++98
    "-Wno-c++98-compat-pedantic",
    # Turns off all implicit conversion warnings. Most are re-enabled below.
    "-Wno-conversion",
    "-Wno-covered-switch-default",
    "-Wno-deprecated",
    "-Wno-disabled-macro-expansion",
    "-Wno-double-promotion",
    ###
    # Turned off as they include valid C++ code.
    "-Wno-comma",
    "-Wno-extra-semi",
    "-Wno-extra-semi-stmt",
    "-Wno-packed",
    "-Wno-padded",
    ###
    # Google style does not use unsigned integers, though STL containers
    # have unsigned types.
    "-Wno-sign-compare",
    ###
    "-Wno-float-conversion",
    "-Wno-float-equal",
    "-Wno-format-nonliteral",
    # Too aggressive: warns on Clang extensions enclosed in Clang-only
    # compilation paths.
    "-Wno-gcc-compat",
    ###
    # Some internal globals are necessary. Don't do this at home.
    "-Wno-global-constructors",
    "-Wno-exit-time-destructors",
    ###
    "-Wno-nested-anon-types",
    "-Wno-non-modular-include-in-module",
    "-Wno-old-style-cast",
    # Warns on preferred usage of non-POD types such as string_view
    "-Wno-range-loop-analysis",
    "-Wno-reserved-id-macro",
    "-Wno-shorten-64-to-32",
    "-Wno-switch-enum",
    "-Wno-thread-safety-negative",
    "-Wno-unknown-warning-option",
    "-Wno-unreachable-code",
    # Causes warnings on include guards
    "-Wno-unused-macros",
    "-Wno-weak-vtables",
    # Causes warnings on usage of types/compare.h comparison operators.
    "-Wno-zero-as-null-pointer-constant",
    ###
    # Implicit conversion warnings turned off by -Wno-conversion
    # which are re-enabled below.
    "-Wbitfield-enum-conversion",
    "-Wbool-conversion",
    "-Wconstant-conversion",
    "-Wenum-conversion",
    "-Wint-conversion",
    "-Wliteral-conversion",
    "-Wnon-literal-null-conversion",
    "-Wnull-conversion",
    "-Wobjc-literal-conversion",
    "-Wno-sign-conversion",
    "-Wstring-conversion",
]

LLVM_TEST_DISABLE_WARNINGS_FLAGS = [
    "-Wno-c99-extensions",
    "-Wno-deprecated-declarations",
    "-Wno-missing-noreturn",
    "-Wno-missing-prototypes",
    "-Wno-missing-variable-declarations",
    "-Wno-null-conversion",
    "-Wno-shadow",
    "-Wno-shift-sign-overflow",
    "-Wno-sign-compare",
    "-Wno-unused-function",
    "-Wno-unused-member-function",
    "-Wno-unused-parameter",
    "-Wno-unused-private-field",
    "-Wno-unused-template",
    "-Wno-used-but-marked-unused",
    "-Wno-zero-as-null-pointer-constant",
    # gtest depends on this GNU extension being offered.
    "-Wno-gnu-zero-variadic-macro-arguments",
]

MSVC_STYLE_EXCEPTIONS_FLAGS = [
    "/U_HAS_EXCEPTIONS",
    "/D_HAS_EXCEPTIONS=1",
    "/EHsc"
]

MSVC_DEFINES = [
    "/DNOMINMAX",  # Don't define min and max macros (windows.h)
    # Don't bloat namespace with incompatible winsock versions.
    "/DWIN32_LEAN_AND_MEAN",
    # Don't warn about usage of insecure C functions.
    "/D_CRT_SECURE_NO_WARNINGS",
    "/D_SCL_SECURE_NO_WARNINGS",
    # Introduced in VS 2017 15.8, allow overaligned types in aligned_storage
    "/D_ENABLE_EXTENDED_ALIGNED_STORAGE",
]

COPT_VARS = {
    "ABSL_GCC_FLAGS": [
        "-Wall",
        "-Wextra",
        "-Wcast-qual",
        "-Wconversion-null",
        "-Wmissing-declarations",
        "-Woverlength-strings",
        "-Wpointer-arith",
        "-Wunused-local-typedefs",
        "-Wunused-result",
        "-Wvarargs",
        "-Wvla",  # variable-length array
        "-Wwrite-strings",
        # gcc-4.x has spurious missing field initializer warnings.
        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=36750
        # Remove when gcc-4.x is no longer supported.
        "-Wno-missing-field-initializers",
        # Google style does not use unsigned integers, though STL containers
        # have unsigned types.
        "-Wno-sign-compare",
    ],
    "ABSL_GCC_TEST_FLAGS": [
        "-Wno-conversion-null",
        "-Wno-deprecated-declarations",
        "-Wno-missing-declarations",
        "-Wno-sign-compare",
        "-Wno-unused-function",
        "-Wno-unused-parameter",
        "-Wno-unused-private-field",
    ],
    "ABSL_GCC_EXCEPTIONS_FLAGS": ["-fexceptions"],
    "ABSL_LLVM_FLAGS":
        LLVM_BIG_WARNING_FLAGS + LLVM_DISABLE_WARNINGS_FLAGS,
    "ABSL_LLVM_TEST_FLAGS":
        LLVM_TEST_DISABLE_WARNINGS_FLAGS,
    "ABSL_LLVM_EXCEPTIONS_FLAGS": ["-fexceptions"],
    "ABSL_CLANG_CL_FLAGS":
        (MSVC_BIG_WARNING_FLAGS + LLVM_DISABLE_WARNINGS_FLAGS + MSVC_DEFINES),
    "ABSL_CLANG_CL_TEST_FLAGS":
        LLVM_TEST_DISABLE_WARNINGS_FLAGS,
    "ABSL_CLANG_CL_EXCEPTIONS_FLAGS":
        MSVC_STYLE_EXCEPTIONS_FLAGS,
    "ABSL_MSVC_FLAGS":
        MSVC_BIG_WARNING_FLAGS + MSVC_DEFINES + [
            "/wd4005",  # macro-redefinition
            "/wd4068",  # unknown pragma
            # qualifier applied to function type has no meaning; ignored
            "/wd4180",
            # conversion from 'type1' to 'type2', possible loss of data
            "/wd4244",
            # conversion from 'size_t' to 'type', possible loss of data
            "/wd4267",
            # The decorated name was longer than the compiler limit
            "/wd4503",
            # forcing value to bool 'true' or 'false' (performance warning)
            "/wd4800",
        ],
    "ABSL_MSVC_TEST_FLAGS": [
        "/wd4018",  # signed/unsigned mismatch
        "/wd4101",  # unreferenced local variable
        "/wd4503",  # decorated name length exceeded, name was truncated
        "/wd4996",  # use of deprecated symbol
        "/DNOMINMAX",  # disable the min() and max() macros from <windows.h>
    ],
    "ABSL_MSVC_EXCEPTIONS_FLAGS":
        MSVC_STYLE_EXCEPTIONS_FLAGS,
    "ABSL_MSVC_LINKOPTS": [
        # Object file doesn't export any previously undefined symbols
        "-ignore:4221",
    ],
    # "HWAES" is an abbreviation for "hardware AES" (AES - Advanced Encryption
    # Standard). These flags are used for detecting whether or not the target
    # architecture has hardware support for AES instructions which can be used
    # to improve performance of some random bit generators.
    "ABSL_RANDOM_HWAES_ARM64_FLAGS": ["-march=armv8-a+crypto"],
    "ABSL_RANDOM_HWAES_ARM32_FLAGS": ["-mfpu=neon"],
    "ABSL_RANDOM_HWAES_X64_FLAGS": [
        "-maes",
        "-msse4.1",
    ],
    "ABSL_RANDOM_HWAES_MSVC_X64_FLAGS": [
        "/O2",  # Maximize speed
        "/Ob2",  # Aggressive inlining
    ],
}
