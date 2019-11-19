# IPC Fuzzer

A Chromium IPC fuzzer is under development by aedla and tsepez. The fuzzer lives
under `src/tools/ipc_fuzzer/` and is running on ClusterFuzz. A previous version
of the fuzzer was a simple bitflipper, which caught around 10 bugs. A new
version is doing smarter mutations and generational fuzzing. To do so, each
`ParamTraits<Type>` needs a corresponding `FuzzTraits<Type>`. Feel free to
contribute.

[TOC]

## Working with the fuzzer

### Build instructions

*   Run `gn args` and add `enable_ipc_fuzzer = true` to your args.gn.
*   build `ipc_fuzzer_all` target
*   component builds are currently broken, sorry
*   Debug builds are broken; only Release mode works.

### Replaying ipcdumps

*   `tools/ipc_fuzzer/scripts/play_testcase.py path/to/testcase.ipcdump`
*   more help: `tools/ipc_fuzzer/scripts/play_testcase.py -h`

### Listing messages in ipcdump

*   `out/<Build>/ipc_message_util --dump path/to/testcase.ipcdump`

### Updating fuzzers in ClusterFuzz

*   `tools/ipc_fuzzer/scripts/cf_package_builder.py`
*   upload `ipc_fuzzer_mut.zip` and `ipc_fuzzer_gen.zip` under build directory
    to ClusterFuzz

### Contributing FuzzTraits

*   add them to `tools/ipc_fuzzer/fuzzer/fuzzer.cc`
*   thanks!

## Components

### ipcdump logger

*   add `enable_ipc_fuzzer = true` to `args.gn`
*   build `chrome` and `ipc_message_dump` targets
*   run chrome with
    `--no-sandbox --ipc-dump-directory=/path/to/ipcdump/directory`
*   ipcdumps will be created in this directory for each renderer using the
    format `_pid_.ipcdump`

### ipcdump replay

Lives under `ipc_fuzzer/replay`. The renderer is replaced with
`ipc_fuzzer_replay` using `--renderer-cmd-prefix`. This is done automatically
with the `ipc_fuzzer/play_testcase.py` convenience script.

### ipcdump mutator / generator

Lives under `ipc_fuzzer/fuzzer`. This is the code that runs on ClusterFuzz. It
uses `FuzzTraits<Type>` to mutate ipcdumps or generate them out of thin air.

## Problems, questions, suggestions

Send them to mbarbella@chromium.org.
