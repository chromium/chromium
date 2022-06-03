Android Deps Repository Generator
---------------------------------

Tool to generate a gradle-specified repository for Android and Java
dependencies.

### Usage

    fetch_all.py [--help]

This script creates a temporary build directory, where it will, for each
of the dependencies specified in `build.gradle`, take care of the following:

  - Download the library
  - Generate a README.chromium file
  - Download the LICENSE
  - Generate a GN target in BUILD.gn
  - Generate .info files for AAR libraries
  - Generate 3pp subdirectories describing the CIPD packages
  - Generate a `deps` entry in DEPS.

It will then compare the build directory with your current workspace, and
print the differences (i.e. new/updated/deleted packages names).

### Adding a new library or updating existing libraries.
Full steps to add a new third party library or update existing libraries:

1. Update `build.gradle` with the new dependency or the new versions.

2. Run `fetch_all.py` to update your current workspace with the changes. This
   will update, among other things, your top-level DEPS file.

3. `git add` all the 3pp related changes and create a CL for review. Keep the
   `3pp/`, `.gradle`, `OWNERS`, `.groovy` changes in the CL and revert the other
   files. The other files should be committed in a follow up CL. Example git commands:
   * `git add third_party/android_deps{*.gradle,*.groovy,*3pp*,*OWNERS,*README.md}`
   * `git commit -m commit_message`
   * `git restore third_party/android_deps DEPS`
   * `git clean -id`

4. Land the first CL in step 3 and wait for the corresponding 3pp packager to
   create the new CIPD packages. The 3pp packager runs every 6 hours. You can
   see the latest runs [here][3pp_bot]. See
   [`//docs/cipd_and_3pp.md`][cipd_and_3pp_doc] for how it works. Anyone on the
   Clank build core team and any trooper can trigger the bot on demand for you.

5. If your follow up CL takes more than a day please revert the original CL.
   Once the bot uploads to cipd there is no need to keep the modified 3pp files.
   The bot runs 4 times a day. When you are ready to land the follow up CL, you
   can land everything together since the cipd packages have already been
   uploaded.

6. Run `fetch_all.py` again. There should not be any 3pp related changes. Create
   a commit.

   If the CL is doing more than upgrading existing packages or adding packages
   from the same source and license (e.g. gms) follow
   [`//docs/adding_to_third_party.md`][docs_link] for the review.

If you are updating any of the gms dependencies, please ensure that the license
file that they use, explained in the [README.chromium][readme_chromium_link] is
up-to-date with the one on android's [website][android_sdk_link], last updated
date is at the bottom.

[3pp_bot]: https://ci.chromium.org/p/chromium/builders/ci/3pp-linux-amd64-packager
[cipd_and_3pp_doc]: ../../docs/cipd_and_3pp.md
[owners_link]: http://go/android-deps-owners
[docs_link]: ../../docs/adding_to_third_party.md
[android_sdk_link]: https://developer.android.com/studio/terms
[readme_chromium_link]: ./README.chromium

### Implementation notes:
The script invokes a Gradle plugin to leverage its dependency resolution
features. An alternative way to implement it is to mix gradle to purely fetch
dependencies and their pom.xml files, and use Python to process and generate
the files. This approach was not as successful, as some information about the
dependencies does not seem to be available purely from the POM file, which
resulted in expecting dependencies that gradle considered unnecessary. This is
especially true nowadays that pom.xml files for many dependencies are no longer
maintained by the package authors.

#### Groovy Style Guide
The groovy code in `//third_party/android_deps/buildSrc/src/main/groovy` loosely
follows the [Groovy Style Guide][groovy_style_guide], and can be linted by each
dev with [npm-groovy-lint][npm_groovy_lint] via either:
- Command Line
`npm-groovy-lint -p third_party/android_deps/buildSrc/src/main/groovy/ --config ~/.groovylintrc.json`
npm-groovy-lint can be installed via `npm install -g npm-groovy-lint`.
- [VS Code extension][vs_code_groovy_lint].

The line length limit for groovy is **120** characters.

Here is a sample `.groovylintrc.json` file:

```
{
    "extends": "recommended",
    "rules": {
        "CatchException": "off",
        "CompileStatic": "off",
        "DuplicateMapLiteral": "off",
        "DuplicateNumberLiteral": "off",
        "DuplicateStringLiteral": "off",
        "FactoryMethodName": "off",
        "JUnitPublicProperty": "off",
        "JavaIoPackageAccess": "off",
        "MethodCount": "off",
        "NestedForLoop": "off",
        "ThrowRuntimeException": "off",
        "formatting.Indentation": {
            "spacesPerIndentLevel": 4
        }
    }
}
```

This is a list of rule names: [Groovy Rule Index by Name][groovy_rule_index].

[groovy_style_guide]: https://groovy-lang.org/style-guide.html
[npm_groovy_lint]: https://github.com/nvuillam/npm-groovy-lint
[vs_code_groovy_lint]: https://marketplace.visualstudio.com/items?itemName=NicolasVuillamy.vscode-groovy-lint
[groovy_rule_index]: https://codenarc.org/codenarc-rule-index-by-name.html
