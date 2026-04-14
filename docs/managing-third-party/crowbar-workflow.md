# Using Crowbar Workflow to manage third-party dependencies

Crowbar workflow is intended to replace the current practice of writing bespoke
`update.sh` scripts that clone a repository, apply patches, then check-in the
result into Chromium. It's useful if any of the following applies:

* You only want a few files from the upstream
* You need to apply patches to the dependency
* You need to perform string replacements (e.g. replace `std::span` with
  Chromium's `base::span`)

As a dependency owner, you MUST first structure your dependency to use the
[required directory structure](#Directory-Structure). Then, specify the following
items in a human-readable Crowbar workflow description file `crowbar.txt`.

1. How to fetch the upstream, such as "Fetch Git repo at \<URL\>, use commit
   \<commit\_hash\>".
2. How to transform the source code, such as "copy file to \<path\>", then apply
   \`.patch\` files.
3. How to check for upstream releases, such as "use latest commit in
   \<branch\>", or "newest version matching \<pattern\>".

Conceptually, a Crowbar workflow will:

1. "Fetch" the upstream source code or archive, and save them to a temporary
   `upstream` folder.
2. "Transform" the fetched source code, in the following order:
   1. Creates an **empty** working directory `workdir`
   2. Copies the files you declared in the workflow from the `upstream`
      directory into `workdir`. You can rename a file or flatten the
      directory structure if you wish.
   3. If you wish, apply patch files you declared in the workflow, in the
      `workdir`
3. Copy the content of the `workdir` (after transformations) to the
   dependency's `src/` folder

In Chromium's post-submit continuous integration (CI), we'll run a service that
continuously monitors the upstream for updates. When a new update is detected,
the service updates the fetch instructions to point to the newer release, then
re-runs the transformations, then creates an "uprev" CL to check in the updated
code into Chromium.

## Directory Structure

```
third_party/
  foo/
    README.chromium
    OWNERS
    crowbar.txt     <-- Required, workflow description
    src/            <-- Required, post-transformation code
      LICENSE           <-- Preserve the upstream LICENSE file
      ...               <-- Upstream code
    BUILD.gn        <-- Optional, build rules
    tests/          <-- Optional, integration tests with Chromium
      .clang-format-ignore     <-- Re-enable code formatter
    patches/        <-- Optional, patch files
      abc.patch
      def.patch
```

You MUST clearly separate the "glue code" from "upstream code" (unmodified or
patched). In other words, code that originates from the upstream MUST be stored
inside the `src/` directory.

You MUST NOT re-format upstream code. We have disabled the Chromium's formatter
in the `third_party/` directory, except for Python and Java files.

* If you are adding glue code, such as integration tests or fuzzers, you SHOULD
  re-enable Chromium's formatter by adding \`.clang-format-ignore\` file in the
  glue code's directory, like
  [this one](https://source.chromium.org/chromium/chromium/src/+/main:third_party/ipcz/.clang-format-ignore;drc=c822490a82cdb6ad479159683a92858f7c6f0a58).
* If your dependency is Python source code, please add the dependency's \`src/\`
  path to
  [.yapfignore](https://source.chromium.org/chromium/chromium/src/+/main:.yapfignore;l=15;drc=6f023d4b49e6c6de014298fe784c7e5c622c704f).
* If your dependency is Java source code, please consult us. It's a known-issue
  that Chromium's Java formatter isn't configurable and will always format
  according to Google Java styleguide.

*** note
Crowbar tooling and workflow specification is a work-in-progress. We aim to
provide Crowbar CLI (and the workflow specification) that runs on a developer's
workstation in Q2 2026. We will update this documentation in the coming months.

For now, please use a human readable `crowbar.txt` to document the steps you
used to produce the content in `src/` directory.

In Q3 2026, we will use AI Agents to convert the `crowbar.txt` description into
a Crowbar workflow. We'll then onboard your dependency to automated updates.

If we can verify the steps your document in `crowbar.txt` produces the contents
of `src/` directory, no actions will be required from you. Otherwise, our team
will reach out for more information.
***

