# Glossary

This page provides definitions of phrases and terminology used in Chromium's
[continuous integration](https://en.wikipedia.org/wiki/Continuous_integration)
infrastructure.


## Builds

* __Build__: A Buildbucket job that runs a single CI-specific task. Composed of
  multiple steps rendered in Milo (e.g. a step for 'fetching the source', a step
  for 'compile', etc.). A build is associated with a single Builder.
  As of writing (Q3 2022), all Buildbucket builds are themselves Swarming tasks.

* __Builder__: A named configuration (or grouping) of builds in Buildbucket. All
  builds of a builder share the same config. For the most part, this means they
  run the same recipe and have largely the same set of steps. A builder may also
  be referred to as a __bot__, but that term is ambiguous and should be avoided
  since it can be confused with a __Swarming bot__. (See the [LUCI](#luci)
  section below.)
  * __Builder/Tester Split__: At the end of their builds, some builders may
    trigger builds on other builders. When the parent build only compiles and
    the child build only runs tests, we call this model a "builder/tester
    split". The child is referred to as the tester, and the parent (confusingly)
    referred to as the builder.
  * __CI Builder__ or __Post-submit Builder__: A builder that compiles/tests a
    branch of Chromium. Often triggered when changes to the branch are
    submitted, these post-submit builders run on a continuous basis.
  * __FYI Builder__: A CI builder with reduced priority and impact. Ideally
    used only for experiments or for temporary build/test configs. May also be
    referred to as an "informational" builder.
  * __Trybot__ or __Trybuilder__ or __Pre-submit Builder__: A builder that
    compiles/tests a pending CL before submission.
  * __Tryjob__: A single build of a trybot.

* __Chromium's CQ__: The set of trybots required for any Chromium CL to pass
  before submission. These are mandatory for nearly every CL.
  * __CQ Builder__: One of the mandatory trybots. See
    [here](../../infra/config/generated/cq-builders.md#required-builders) for
    the full list.
  * __Compilator__: A type of builder that handles only the "compiling" and
    "isolating" phases of a CQ build. ("Isolating" referring to the process of
    uploading test binaries to the CAS server. See CAS in the [misc](#misc)
    section below.) The name is a portmanteau of the words "compiler" and
    "isolator".
  * __Orchestrator__: A type of CQ builder that delegates all "compiling" and
    "isolating" phases of a build to a triggered child builder called a
    compilator (see above). For example,
    [linux-rel](https://ci.chromium.org/p/chromium/builders/try/linux-rel) is
    an orchestrator on the CQ and
    [linux-rel-compilator](https://ci.chromium.org/p/chromium/builders/try/linux-rel-compilator)
    is its compilator. This partitioning confines the busy-waiting phases of a
    build (ie: waiting for Swarming tests to finish) to the orchestrator,
    allowing the compilator to quickly move on and pick up new requests.
  * __Optional Trybot__: A trybot that's not a default CQ builder. These can
    be triggered manually by a developer. Or they can be required by the CQ
    if the CL includes changes to a specific file path. See
    [here](../../infra/config/generated/cq-builders.md#optional-builders) for a
    list of the optional trybots and the filepaths that require them. Trybots
    that fall into that latter category can also be referred to as
    __path-based trybots__.
  * __Mega CQ__: An alternative mode of Chromium's CQ that greatly increases the
    amount of trybots triggered. This increases the confidence that a CL won't
    be reverted after landing, but at the cost of much longer CQ cycle times.
    Triggered via the `Mega CQ` buttons in Gerrit. See [CQ docs](cq.md#modes)
    for more info.

* __Builderless__: A Swarming dimension (see below for definition of a
    dimension) that simply indicates the Swarming bot doesn't exclusively run
    builds for a dedicated set of builders. Instead, it belongs to a generic
    pool of bots (colloquially known as the "builderless pool") that's shared
    across a large group of builders. Any builder that runs builds in said
    pool is also referred to as being "builderless."

* __Builder Group__: A logical grouping of builders. For example, the
  "chromium.linux" builder group is a set of builders that test basic
  functionality of Chromium on Linux.

* __Waterfall__: A largely historical term that often refers to a particular
  grouping of Builders or Builder Groups. Examples include:
  * __The Perf Waterfall__: Builders in the
    [chrome.perf](https://ci.chromium.org/p/chrome/g/chrome.perf) builder group.
  * __The Main Waterfall__: Another historical term. Often refers to the set of
    builders whose failures close the
    [Chromium tree](https://chromium-status.appspot.com/). Spans multiple
    builder groups.

## LUCI

LUCI is a collection of continuous integration services running on Google Cloud.
These services largely make-up the backbone of Chromium's CI.

* __Buildbucket__: A LUCI service that tracks builds, and queues them into
  Builders.

* __Change Verifier__ or __CV__: A LUCI service that triggers and manages the
  builds a CL must pass before the CL is submitted into the repository's trunk.
  These builds are known as "pre-submit", and the full set of required builds
  that CV enforces for a repo are that repo's "CQ". (See above for a description
  of Chromium's particular CQ.)

* __CIPD__: A LUCI service for hosting and distributing packages. Packages
  stored in CIPD are arbitrary sets of files. Practically, these are often SDKs,
  toolchains, and other various binaries not suitable for being checked-into a
  Git repo directly.

* __Milo__: A LUCI service that renders Builds in a web-browser UI. Hosted on
  https://ci.chromium.org. Alliases also include https://build.chromium.org and
  https://luci-milo.appspot.com. Named after the Pokemon
  [Milotic](https://bulbapedia.bulbagarden.net/wiki/Milotic_(Pok%C3%A9mon)).

* __Recipe__: A LUCI technology that defines what steps a build runs. Written in
  a domain-specific language embedded in Python. Nearly all builds run a recipe.
  (The alternative to recipes being a binary that implements the luciexe
  protocol. As of Q3 2022, non-recipe luciexe binaries have limited use in
  Chromium's infra.)

* __Swarming__: A LUCI service that runs arbitrary tasks across a fleet of
  workers knows as Swarming bots.
  * __Swarming bot__: A single worker that communicates with the Swarming server
    and executes Swarming tasks. The worker often, but not necessarily, runs on
    a Linux, Mac, or Win VM. A bot belongs to one or more Swarming pools.
  * __Swarming dimension__: A key:value pair that a Swarming bot tags to itself
    to describe its hardware or software attributes. (e.g. A machine running
    macOS Ventura would tag itself with `os:Mac-13`.) A task can then include
    certain dimensions in its request, and only bots that match all requested
    dimensions will execute the task.
  * __Swarming pool__: A collection of Swarming bots. A pool acts as a security
    boundary, enforcing who can view and trigger tasks on its bots.
  * __Swarming task__: A single workload request. A task defines a command to
    run, and is associated to a single Swarming pool.

## Misc

* __CAS__: Content Addresses Storage, a service for storing binary blobs. Used
  on Chromium's infra to store and transfer test inputs from the builder that
  compiles the tests to the Swarming bots that run them. May also be referred
  to by its predecessor's name: "isolate".
* __Gerrit__: The code-review tool for changes to Google-hosted Git
  repositoris. See chromium/src.git's Gerrit instance
  [here](https://chromium-review.googlesource.com/).
* __Gitiles__: The web-browser UI for Google-hosted Git repositories. See
  chromium/src.git on gitiles
  [here](https://chromium.googlesource.com/chromium/src/+/HEAD).
