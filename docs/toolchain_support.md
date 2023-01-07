Configurations supported by the toolchain team
==============================================

This document describes considerations to follow when adding a new build
config. A build config is something like a new compiler / linker configuration,
a new test binary, a new `target_os`, a new `target_cpu`, etc.

Background
----------

We update our toolchain (the C/C++/Objective-C compiler `clang`, the linker
`lld`, and a small assortment of helper binaries) every 2-4 weeks.

This toolchain is used to build Chromium for **7+ platforms** (Android,
Chromecast, Chrome OS, Fuchsia, iOS, Linux, macOS, Windows) targeting
**4+ CPUs** (arm, arm64, x86, x64) in **6+ build modes** (debug (component
non-optimized), release (static build optimized), official build (very
optimized and on some platforms LTO+CFI), asan+lsan, msan, tsan), resulting
in **130+** different test binaries.

Every toolchain update needs to make sure that none of these combinations break.

To have any chance that this works, we continuously build and run tests in
most of these configurations with trunk clang/llvm, to catch regressions and
intentional changes upstream that cause problems for us.

When we land a toolchain update, we rely on the CQ to make sure all combinations
work with the new toolchain. We use all default CQ bots, and a long list of
opt-in trybots.

The toolchain team has established contacts to most platform owners in
Chromium, so that we can ask for help quickly when needed.


Toolchain guarantees
--------------------

For configurations that have a bot on the [chromium.clang waterfall](
https://ci.chromium.org/p/chromium/g/chromium.clang/console) (which
is where all the bots are that test Chromium with trunk clang/llvm)
and that are either part of the default CQ or that have an opt-in bot
that's [used on clang rolls](https://cs.chromium.org/chromium/src/tools/clang/scripts/upload_revision.py?q=upload_revi&sq=package:chromium&g=0&l=33),
we guarantee that we won't land a toolchain update that breaks that
configuration.

For configurations that don't have a clang tip-of-tree (ToT) bot or that aren't
covered on the CQ, **we won't revert toolchain updates**. We will do our best
to fix things quickly (see below for how to file a good bug) and to fix forward
to get you unblocked.


Talk to the toolchain team to make sure your new config is supported
--------------------------------------------------------------------

If you add a new build config, or a new bot config: You may want to add a
chromium.clang ToT bot, and you may want to make sure that there's a CQ bot
covering your config on clang rolls. (It's ok if it's an opt-in bot, as long as
you make sure it's
[opted-in](https://cs.chromium.org/chromium/src/tools/clang/scripts/upload_revision.py?q=upload_revi&sq=package:chromium&g=0&l=33)
for clang rolls. If your opt-in is based on filename patterns, make sure it
also fires on changes to `tools/clang/scrips/update.py`.)

Do not use `-mllvm` or `-Xclang` flags. These are internal flags that aren't
ready for production use yet. Once they're ready, they'll become available
as regular clang flags.

Follow the style guide. In particular, don't use exceptions.

Talk to us if you're adding a new build config or bot config, if you'd like to
use an internal flag, if you want to use a flag that's obscure, or if you want
general advice on toolchain questions (clang@chromium.org, or
google-internally, lexan@google.com).

Filing good toolchain bugs
--------------------------

If a toolchain update ("clang roll") broke you, here's how you can file a bug
that we can act on the quickest:

- File the bug in the `Tools>LLVM` component.
- Link to the CL with the toolchain update that broke you.
- Link to a specific build showing the breakage, ideally the first instance
  of the breakage.
- If reproducing your problem requires more than a regular Chromium checkout
  and replicating what the bot you linked to does, or if you can't link to a
  build: Provide commands on how to reproduce your problem, targeted at someone
  who knows the chromium build well but doesn't know your feature / platform
  at all.
  - Tell us which repo to check out, if needed.
  - Tell us which `args.gn` to use.
  - Tell us which target to build.
  - Tell us how to run your test.

We'll try to be helpful, but see "Toolchain guarantees" above.

Compiler updates can expose latent existing bugs in your code, for example
if you have ODR violations, or other undefined behavior. In that case,
the fix is to change your code. It can be helpful to make a reduced repro
case of the problem before looping us in, so that you can check if your problem
is really due to the toolchain update and not due to a bug in your code.
