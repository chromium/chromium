# Using LED

LED is an infrastructure tool used to manually trigger builds on any builder
running on LUCI. It's designed to help debug build failures or experiment with
new builder changes. This doc describes how to use it with Chromium's builders.

[TOC]

## When to use it

Use cases include, but are not limited to, the following:
* **Testing a recipe change**: Much of the code in the following repos
define what and how a builder runs (also known as the builder's "recipe"):
[recipes-py][1], [depot_tools][2], and [tools/build][3]. Changes to these
repos can first be tested out on any builder prior to submitting via LED.
* **Debugging a waterfall failure**: If a waterfall builder (that is, *not* a
trybot) is exhibitting frequent or strange failures that can't be reproduced
locally, LED can be used to retrigger any given build for debugging.

## When *not* to use it

Certain types of changes to a trybot (this includes all builders on the CQ)
can be sufficiently tested without the use of LED. This includes changes to a
trybot's:
* **GN args**: A trybot's build args are configured via
[mb_config.pyl][4].
* **tests**: The list of tests a trybot runs are set via the \*.pyl files in
[//testing/buildbot/][5]. (Some trybots may not be present in
those files. Instead, change the waterfall builders they mirror. This mapping is
configured in tools/build's [trybots.py][6].)

Simply edit the needed files in a local chromium/src checkout, upload the change
to Gerrit, then select the affected trybot(s) via the "select tryjobs" menu.

## How to use it

Provided that a local depot_tools checkout is present on $PATH, LED can be
used by simply invoking `led` on the command line. A common use-case for LED is
to modify the build steps of any given builder. The process for doing this is
outlined below. (However, LED can be used for many other purposes. See the full
list of features via `led help`.)

1. Select a builder whose builds you'd like to reproduce. (Example:
[linux-rel][7])
2. Record its full builder name, along with its bucket. (The bucket name is
present in the URL of the builder page, and is very likely "chromium/ci".)
3. Checkout the [tools/build][3] repo (if not already present) and navigate to
the [chromium][8] and/or [chromium_tests][9] recipe modules. These, along with
the other recipe_modules located in tools/build, are how the majority of a
Chromium builder's recipe is defined.
4. Make the desired recipe change. (Consider running local recipe unittests
before proceeding by running `recipes.py test train` via the [recipes.py][10]
script.
5. Launch a build with the given recipe change. This can be done with a single
chained LED invocation, eg:
`led get-builder chromium/ci:linux-rel | led edit-recipe-bundle | led launch`
6. The LED invocation above will print out a link to the build that was
launched. Repeat steps 4 & 5 until the triggered builds behave as expected
with the new recipe change.

## Questions? Feedback?

If you're in need of further assistance, if you're not sure about
one or more steps, or if you found this documentation lacking, please
reach out to infra-dev@chromium.org or [file a bug][11]!

[1]: https://chromium.googlesource.com/infra/luci/recipes-py/
[2]: https://chromium.googlesource.com/chromium/tools/depot_tools/
[3]: https://chromium.googlesource.com/chromium/tools/build/
[4]: /tools/mb/mb_config.pyl
[5]: /testing/buildbot/
[6]: https://chromium.googlesource.com/chromium/tools/build/+/HEAD/scripts/slave/recipe_modules/chromium_tests/trybots.py
[7]: https://ci.chromium.org/p/chromium/builders/ci/linux-rel
[8]: https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipe_modules/chromium/api.py
[9]: https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipe_modules/chromium_tests/api.py
[10]: https://chromium.googlesource.com/chromium/tools/build/+/master/scripts/slave/recipes.py
[11]: https://g.co/bugatrooper
