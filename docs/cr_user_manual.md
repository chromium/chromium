# Cr

Cr is a tool that tries to hide some of the tools used for working on Chromium
behind an abstraction layer. Nothing that cr does can't be done manually, but cr
attempts to make things nicer. Its main additional feature is that it allows you
to build many configurations and run targets within a single checkout (by not
relying on a directory called 'out'). This is especially important when you want
to cross-compile (for instance, building Android from Linux or building arm from
intel), but it extends to any build variation.

[TOC]

## A quick example

The following is all you need to prepare an output directory, and then build and
run the release build of chrome for the host platform:

```shell
cr init
cr run
```

## How do I get it?

You already have it, it lives in `src/tools/cr`

You can run the cr.py file by hand, but if you are using bash it is much easier
to source the bash helpers. This will add a cr function to your bash shell that
runs with pyc generation turned off, and also installs the bash tab completion
handler (which is very primitive at the moment, it only completes the command
not the options) It also adds a function you can use in your prompt to tell you
your selected build (`_cr_ps1`), and an helper to return you to the root of your
active tree (`crcd`). I recommend you add the following lines to the end of your
`~/.bashrc` (with a more correct path):

```shell
CR_CLIENT_PATH="/path/to/chromium"
source ${CR_CLIENT_PATH}/src/tools/cr/cr-bash-helpers.sh
```

At that point the cr command is available to you.

## How do I use it?

It should be mostly self documenting

    cr --help

will list all the commands installed

    cr --help command

will give you more detailed help for a specific command.

_**A note to existing Android developers:**_

*   Do not source envsetup! ever!
*   If you use cr in a shell that has had envsetup sourced, miscellaneous things
    will be broken. The cr tool does all the work that envsetup used to do in a
    way that does not pollute your current shell.
*   If you really need a shell that has had the environment modified like
    envsetup used to do, see the cr shell command, also probably file a bug for
    a missing cr feature!

## The commands

Below are some common workflows, but first there is a quick overview of the
commands currently in the system.

### Output directory commands

    init

Create and configure an output directory. Also runs select to make it the
default.

    select

Select an output directory. This makes it the default output for all commands,
so you can omit  the --out option if you want to.

    prepare

Prepares an output directory. This runs any preparation steps needed for an
output directory to be viable, which at the moment means run gyp.

### Build commands

    build

Build a target.

    install

Install a binary. Does build first unless `--builder==skip`. This does nothing
on Linux, and installs the apk onto the device for Android builds.

    run

Invoke a target. Does an install first, unless `--installer=skip`.

    debug

Debug a target. Does a run first, unless `--runner=skip`. Uses the debugger
specified by `--debugger`.

### Other commands

    sync

Sync the source tree. Uses gclient sync to do the real work.

    shell

Run an exernal command in a cr environment. This is an escape hatch, if passed
a command it runs it in the correct environment for the current output
directory, otherwise it starts a sub shell with that environment. This allows
you to run any commands that don't have shims, or are too specialized to get
one. This is especially important on Android where the environment is heavily
modified.

## Preparing to build

The first thing you need to do is prepare an output directory to build into.
You do this with:

    cr init

By default on Linux this will prepare a Linux x86 release build output
directory, called `out_linux/Release`, if you want an Android debug one, you can
use:

    cr init --out=out_android/Debug

The output directory can be called anything you like, but if you pick a non
standard name cr might not be able to infer the platform, in which case you need
to specify it. The second part **must** be either Release or Debug. All options
can be shortened to the shortest non ambiguous prefix, so the short command line
to prepare an Android debug output directory in out is:

    cr init --o=out/Debug --p=android

It is totally safe to do this in an existing output directory, and is an easy
way to migrate an existing output directory to use in cr if you don't want to
start from scratch.

Most commands in cr take a --out parameter to tell them which output directory
you want to operate on, but it will default to the last value passed to init or
select. This enables you to omit it from most commands.

Both init and select do additional work to prepare the output directory, which
include things like running gyp. You can do that work on its own with the
prepare command if you want, something you need to do when changing between
branches where you have modified the build files.

If you want to set custom GYP defines for your build you can do this by adding
adding the `-s GYP_DEFINES` argument, for example:

    cr init --o=out/Debug -s GYP_DEFINES=component=shared_library

## Running chrome

If you just want to do a basic build and run, then you do

    cr run

which will build, install if necessary, and run chrome, with some default args
to open on https://www.google.com/. The same command line will work for any
supported platform and mode. If you want to just run it again, you can turn off
the build and install steps,

    cr run --installer=skip

note that turning off install automatically turns off build (which you could do
with `--builder=skip`) as there is no point building if you are not going to
install.

## Debugging chrome

To start chrome under a debugger you use

    cr debug

which will build, install, and run chrome, and attach a debugger to it. This
works on any supported platform, and if multiple debuggers are available, you
can select which one you want with `--debugger=my_debugger`

## Help, it went wrong!

There are a few things to do, and you should probably do all of them.
Run your commands with dry-run and/or verbose turned on to see what the tool is
really doing, for instance

    cr --d -vvvv init

The number of v's matter, it's the verbosity level, you can also just specify
the value with -v=4 if you would rather.

[Report a bug], even if it is just something that confused or annoyed rather
than broke, we want this tool to be very low friction for developers.

## Known issues

You can see the full list of issues with
[this](https://code.google.com/p/chromium/issues/list?can=2&q=label%3Atool-cr)
query, but here are the high level issues:

*   **Only supports gtest** : You run tests using the run command, which tries
    to infer from the target whether it is a runnable binary or a test. The
    inference could be improved, and it needs to handle the other test types as
    well.
*   **No support for Windows or Mac** : allowed for in the design, but need
    people with expertise on those platforms to help out
*   **Bash completion** : The hooks for it are there, but at the moment it only
    ever completes the command, not any of the arguments

[Report a bug]:
https://code.google.com/p/chromium/issues/entry?comment=%3CDont%20forget%20to%20attach%20the%20command%20lines%20used%20with%20-v=4%20if%20possible%3E&pri=2&labels=OS-Android,tool-cr,Build-Tools,Type-Bug&owner=iancottrell@chromium.org&status=Assigned
