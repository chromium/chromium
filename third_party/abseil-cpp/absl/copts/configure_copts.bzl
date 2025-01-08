"""absl specific copts.

This file simply selects the correct options from the generated files.  To
change Abseil copts, edit absl/copts/copts.py
"""

load(
    "//absl:copts/GENERATED_copts.bzl",
    "ABSL_CLANG_CL_FLAGS",
    "ABSL_CLANG_CL_TEST_FLAGS",
    "ABSL_GCC_FLAGS",
    "ABSL_GCC_TEST_FLAGS",
    "ABSL_LLVM_FLAGS",
    "ABSL_LLVM_TEST_FLAGS",
    "ABSL_MSVC_FLAGS",
    "ABSL_MSVC_LINKOPTS",
    "ABSL_MSVC_TEST_FLAGS",
)

ABSL_DEFAULT_COPTS = select({
    "//absl:msvc_compiler": ABSL_MSVC_FLAGS,
    "//absl:clang-cl_compiler": ABSL_CLANG_CL_FLAGS,
    "//absl:clang_compiler": ABSL_LLVM_FLAGS,
    "//absl:gcc_compiler": ABSL_GCC_FLAGS,
    "//conditions:default": ABSL_GCC_FLAGS,
})

ABSL_TEST_COPTS = select({
    "//absl:msvc_compiler": ABSL_MSVC_TEST_FLAGS,
    "//absl:clang-cl_compiler": ABSL_CLANG_CL_TEST_FLAGS,
    "//absl:clang_compiler": ABSL_LLVM_TEST_FLAGS,
    "//absl:gcc_compiler": ABSL_GCC_TEST_FLAGS,
    "//conditions:default": ABSL_GCC_TEST_FLAGS,
})

ABSL_DEFAULT_LINKOPTS = select({
    "//absl:msvc_compiler": ABSL_MSVC_LINKOPTS,
    "//conditions:default": [],
})
