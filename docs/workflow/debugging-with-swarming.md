# Debugging with Swarming

This document outlines how to debug a test failure on a specific builder
configuration on Swarming using the [UTR tool](../../tools/utr/README.md)
without needing to repeatedly upload new CL revisions or do CQ dry runs. This
tool will automatically handle steps like replicating the right GN args,
building & uploading the test isolate, triggering & collecting the swarming test
tasks.

[TOC]

## Overview & Terms

*Swarming* is a system operated by the infra team that schedules and runs tasks
under a specific set of constraints, like "this must run on a macOS 10.13 host"
or "this must run on a host with an intel GPU". It is somewhat similar to part
of [Borg], or to [Kubernetes].

An *isolate* is an archive containing all the files needed to do a specific task
on the swarming infrastructure. It contains binaries as well as any libraries
they link against or support data. An isolate can be thought of like a tarball,
but held by the CAS server and identified by a digest of its contents. The
isolate also includes the command(s) to run, which is why the command is
specified when building the isolate, not when executing it. See the
[infra glossary](../infra/glossary.md) for the definitions of these terms and
more.

Normally, when you do a CQ dry run, something like this happens:

```
  for type in builders_to_run:
    targets = compute_targets_for(type)
    isolates = use_swarming_to_build(type, targets) # uploads isolates for targets
    wait_for_swarming_to_be_done()

    for isolate in isolates:
      use_swarming_to_run(type, isolate) # downloads isolates onto the bots used
    wait_for_swarming_to_be_done()
```

When you do a CQ retry on a specific set of bots, that simply constrains
`builders_to_run` in the pseudocode above. However, if you're trying to rerun a
specific target on a specific bot, because you're trying to reproduce a failure
or debug, doing a CQ retry will still waste a lot of time - the retry will still
build and run *all* targets, even if it's only for one bot.

Fortunately, you can manually invoke some steps of this process. What you really
want to do is:

```
  isolate = use_swarming_to_build(type, target) # can't do this yet, see below
  use_swarming_to_run(type, isolate)
```

or perhaps:

```
  isolate = upload_to_cas(target_you_built_locally)
  use_swarming_to_run(type, isolate)
```

## A concrete example

Here's how to run `chrome_public_unit_test_apk` on Android devices. By using the
config of the `android-arm64-rel` trybot, we can run it on Pixel 3 XLs running
Android Pie.

```sh
$ vpython3 tools/utr \
    -p chromium \
    -B try \
    -b android-arm64-rel \
    -t "chrome_public_unit_test_apk on Android device Pixel 3 XL" \
    compile-and-test
```

You can find the UTR invocation for any test on the build UI under the step's
"reproduction instructions" (displayed by clicking the page icon in the UI).

## Other notes

If you are looking at a Swarming task page, be sure to check the bottom of the
page, which gives you commands to:

* Download the contents of the isolate the task used
* Reproduce the task's configuration locally
* Download all output results from the task locally

[borg]: https://ai.google/research/pubs/pub43438
[kubernetes]: https://kubernetes.io/
[swarming bot list]: https://chromium-swarm.appspot.com/botlist

To find out repo checkout, gn args, etc for local compile, you can use
[how to repro bot failures](../testing/how_to_repro_bot_failures.md)
as a reference.
