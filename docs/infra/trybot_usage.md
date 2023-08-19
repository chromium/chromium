# Using the trybots

[TOC]

## Overview

The trybots let committers try uncommitted patches on multiple platforms in an
automated way.

-   Trybots include all platforms for which we currently build Chromium, though
    they may not support all configurations built on CI.
-   The commit queue (CQ) runs a subset of available trybots. See [here][1] for
    more information.
-   Trybots can be manually invoked via `git cl try` or the "Choose Tryjobs" UI
    in gerrit.
-   Custom trybots can be added to the list of builders triggered and checked by
    the CQ via the `Cq-Include-Trybots` commit message footer. See [here][2]
    for more information.
-   Any committer can use the trybots.
-   Non-committers with tryjob access can also use the trybots. See [here][3]
    for more information.
-   External contributors without tryjob access can ask committers to run
    tryjobs for them.

*** note
**Warning**: Please do not trigger more than ~5-10 tryjobs per builder
per hour. We don't have enough spare capacity for more than that, and we don't
have per-user quotas yet (https://crbug.com/1091070 to implement that).
***

## Workflow

1.  Upload your change to gerrit via `git cl upload`
2.  Run trybots:

    *   Run the default set of trybots by starting a CQ dry run, either by
        setting CQ+1 on gerrit or by running `git cl try` with no arguments.
    *   Manually run trybots of your choice by either the "Choose Tryjobs"
        button in gerrit or providing arguments to `git cl try`:

        *   specify bucket name with `-B/--bucket`. For chromium tryjobs, this
            should always be `luci.chromium.try`
        *   specify bot names with `-b/--bot`. This can be specified more than
            once.

### Examples

Launching a CQ dry run:

```bash
$ git cl try
```

Launching a particular trybot:

```bash
$ git cl try -B luci.chromium.try -b linux-rel
```

Launching multiple trybots:

```bash
$ git cl try -B luci.chromium.try \
  -b android-binary-size \
  -b ios-simulator-full-configs \
  -b linux-blink-rel \
  -b win7-blink-rel
  # etc
```

## Trying Changes in Dependencies

It is also possible to run a Chromium try job with a pending CL in a separate
repository that is synced via DEPS. Normally DEPS files specify the SHA1
revision hash of the dependency. But commits that are part of pending CLs are
not part of the default
[refspec](https://git-scm.com/book/en/v2/Git-Internals-The-Refspec) fetched when
gclient checks out the dependency. Instead, you can specify a symbolic reference
to your change, like `refs/changes/12/2277112/3`. To determine the ref to use,
click the "Download" button on the dependency CL in Gerrit, which will show it
as part of several git commands. Then edit the DEPS file in Chromium.

If, for example, you wanted to test a pending V8 CL in Chromium, you would edit
the DEPS line, which may look like this:

```
  'v8_revision': '50bc0b22b15da1410a1be6240a25a184d5896908',
```

And change it to:

```
  'v8_revision': 'refs/changes/12/2277112/3',
```

When you run the try job, gclient will sync in your pending CL. Note that if
your pending CL is based on a revision that is either older or newer than the
revision specified in DEPS, the tryjob may fail. You can rebase your CL to be on
top of the same revision specified in the DEPS file to avoid this.

## Bugs? Feature requests? Questions?

[File a trooper bug.]

[1]: /docs/infra/cq.md
[2]: /docs/contributing.md#cl-footer-reference
[3]: https://www.chromium.org/getting-involved/become-a-committer#TOC-Try-job-access
[File a trooper bug.]: https://g.co/bugatrooper
