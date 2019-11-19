# Clang Sheriffing

Chromium bundles its own pre-built version of [Clang](clang.md). This is done so
that Chromium developers have access to the latest and greatest developer tools
provided by Clang and LLVM (ASan, CFI, coverage, etc). In order to [update the
compiler](updating_clang.md) (roll clang), it has to be tested so that we can be
confident that it works in the configurations that Chromium cares about.

We maintain a [waterfall of
builders](https://ci.chromium.org/p/chromium/g/chromium.clang/console) that
continuously build fresh versions of Clang and use them to build and test
Chromium. "Clang sheriffing" is the process of monitoring that waterfall,
determining if any compile or test failures are due to an upstream compiler
change, filing bugs upstream, and often reverting bad changes in LLVM. This
document describes some of the processes and techniques for doing that.

https://sheriff-o-matic.appspot.com/chromium.clang is the sheriff-o-matic
view of that waterfall, which can be easier to work with.

[TOC]

## Is it the compiler?

Chromium does not always build and pass tests in all configurations that
everyone cares about. Some configurations simply take too long to build
(ThinLTO) or be tested (dbg) on the CQ before committing. And, some tests are
flaky. So, our console is often filled with red boxes, and the boxes don't
always need to be green to roll clang.

Oftentimes, if a bot is red with a test failure, it's not a bug in the compiler.
To check this, the easiest and best thing to do is to try to find a
corresponding builder that doesn't use ToT clang. For standard configurations,
start on the waterfall that corresponds to the OS of the red bot, and search
from there. If the failing bot is Google Chrome branded, go to the (Google
internal) [official builder
list](https://uberchromegw.corp.google.com/i/official.desktop.continuous/builders/)
and start searching from there.

If you are feeling charitable, you can try to see when the test failure was
introduced by looking at the history in the bot. One way to do this is to add
`?numbuilds=200` to the builder URL to see more history. If that isn't enough
history, you can manually binary search build numbers by editing the URL until
you find where the regression was introduced. If it's immediately clear what CL
introduced the regression (i.e.  caused tests to fail reliably in the official
build configuration), you can simply load the change in gerrit and revert it,
linking to the first failing build that implicates the change being reverted.

If the failure looks like a compiler bug, these are the common failures we see
and what to do about them:

1. compiler crash
1. compiler warning change
1. compiler error
1. miscompile
1. linker errors

## Compiler crash

This is probably the most common bug. The standard procedure is to do these
things:

1. Use `got_clang_revision` property from first red and last green build to find
   upstream regression range
1. File a crbug documenting the crash. Include the range, and any other bots
   displaying the same symptoms.
1. All clang crashes on the Chromium bots are automatically uploaded to
   Cloud Storage. On the failing build, click the "stdout" link of the
   "process clang crashes" step right after the red compile step. It will
   print something like

       processing heap_page-65b34d... compressing... uploading... done
           gs://chrome-clang-crash-reports/v1/2019/08/27/chromium.clang-ToTMac-20955-heap_page-65b34d.tgz
       removing heap_page-65b34d.sh
       removing heap_page-65b34d.cpp

   Use
   `gsutil.py cp gs://chrome-clang-crash-reports/v1/2019/08/27/chromium.clang-ToTMac-20955-heap_page-65b34d.tgz .`
   to copy it to your local machine. Untar with
   `tar xzf chromium.clang-ToTMac-20955-heap_page-65b34d.tgz` and change the
   included shell script to point to a locally-built clang. Remove the
   `-Xclang -plugin` flags.  If you re-run the shell script, it should
   reproduce the crash.
1. Identify the revision that introduced the crash. First, look at the commit
   messages in the LLVM revision range to see if one modifies the code near the
   point of the crash. If so, try reverting it locally, rebuild, and run the
   reproducer to see if the crash goes away.

   If that doesn't work, use `git bisect`. Use this as a template for the bisect
   run script:
   ```shell
   #!/bin/bash
   cd $(dirname $0)  # get into llvm build dir
   ninja -j900 clang || exit 125 # skip revisions that don't compile
   ./t-8f292b.sh || exit 1  # exit 0 if good, 1 if bad
   ```
1. File an upstream bug like http://llvm.org/PR43016. Usually the unminimized repro
   is too large for LLVM's bugzilla, so attach it to a (public) crbug and link
   to that from the LLVM bug. Then revert with a commit message like
   "Revert r368987, it caused PR43016."
1. If you want, make a reduced repro using CReduce.  Clang contains a handy wrapper around
   CReduce that you can invoke like so:

       clang/utils/creduce-clang-crash.py --llvm-bin bin \
           angle_deqp_gtest-d421b0.sh angle_deqp_gtest-d421b0.cpp

   Attach the reproducer to the llvm bug you filed in the previous step.

   If you need to do something the wrapper doesn't support,
   follow the [official CReduce docs](https://embed.cs.utah.edu/creduce/using/)
   for writing an interestingness test and use creduce directly.

## Compiler warning change

New Clang versions often find new bad code patterns to warn on. Chromium builds
with `-Werror`, so improvements to warnings often turn into build failures in
Chromium. Once you understand the code pattern Clang is complaining about, file
a bug to do either fix or silence the new warning.

If this is a completely new warning, disable it by adding `-Wno-NEW-WARNING` to
[this list of disabled
warnings](https://cs.chromium.org/chromium/src/build/config/compiler/BUILD.gn?l=1479)
if `llvm_force_head_revision` is true. Here is [an
example](https://chromium-review.googlesource.com/1251622). This will keep the
ToT bots green while you decide what to do.

Sometimes, behavior changes and a pre-existing warning changes to warn on new
code. In this case, fixing Chromium may be the easiest and quickest fix. If
there are many sites, you may consider changing clang to put the new diagnostic
into a new warning group so you can handle it as a new warning as described
above.

If the warning is high value, then eventually our team or other contributors
will end up fixing the crbug and there is nothing more to do.  If the warning
seems low value, pass that feedback along to the author of the new warning
upstream. It's unlikely that it should be on by default or enabled by `-Wall` if
users don't find it valuable. If the warning is particularly noisy and can't be
easily disabled without disabling other high value warnings, you should consider
reverting the change upstream and asking for more discussion.

## Compiler error

This rarely happens, but sometimes clang becomes more strict and no longer
accepts code that it previously did. The standard procedure for a new warning
may apply, but it's more likely that the upstream Clang change should be
reverted, if the C++ code in question in Chromium looks valid.

## Miscompile

Miscompiles tend to result in crashes, so if you see a test with the CRASHED
status, this is probably what you want to do.

1. Bisect object files to find the object with the code that changed.
1. Debug it with a traditional debugger

## Linker error

`ld.lld`'s `--reproduce` flag makes LLD write a tar archive of all its inputs
and a file `response.txt` that contains the link command. This allows people to
work on linker bugs without having to have a Chromium build environment.

To use `ld.lld`'s `--reproduce` flag, follow these steps:

1. Locally (build Chromium with a locally-built
   clang)[https://chromium.googlesource.com/chromium/src.git/+/master/docs/clang.md#Using-a-custom-clang-binary]

1. After reproducing the link error, build just the failing target with
   ninja's `-v -d keeprsp` flags added:
  `ninja -C out/gn base_unittests -v -d keeprsp`.

1. Copy the link command that ninja prints, `cd out/gn`, paste it, and manually
   append `-Wl,--reproduce,repro.tar`. With `lld-link`, instead append
   `/linkrepro:repro.tar`. (`ld.lld` is invoked through the `clang` driver, so
   it needs `-Wl` to pass the flag through to the linker. `lld-link` is called
   directly, so the flag needs no prefix.)

1. Zip up the tar file: `gzip repro.tar`. This will take a few minutes and
   produce a .tar.gz file that's 0.5-1 GB.

1. Upload the .tar.gz to Google Drive. If you're signed in with your @google
   address, you won't be able to make a world-shareable link to it, so upload
   it in a Window where you're signed in with your @chromium account.

1. File an LLVM bug linking to the file. Example: http://llvm.org/PR43241

TODO: Describe object file bisection, identify obj with symbol that no longer
has the section.

## Tips and tricks

Finding what object files differ between two directories:

```
$ diff -u <(cd out.good && find . -name "*.o" -exec sha1sum {} \; | sort -k2) \
          <(cd out.bad  && find . -name "*.o" -exec sha1sum {} \; | sort -k2)
```

Or with cmp:

```
$ find good -name "*.o" -exec bash -c 'cmp -s $0 ${0/good/bad} || echo $0' {} \;
```
