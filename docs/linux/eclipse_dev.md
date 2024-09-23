# Linux Eclipse Dev

Eclipse can be used on Linux (and probably Windows and Mac) as an IDE for
developing Chromium. It's unpolished, but here's what works:

*   Editing code works well (especially if you're used to it or Visual Studio).
*   Navigating around the code works well. There are multiple ways to do this
    (F3, control-click, outlines).
*   Building works fairly well and it does a decent job of parsing errors so
    that you can click and jump to the problem spot.
*   Debugging is hit & miss. You can set breakpoints and view variables. STL
    containers give it (and gdb) a bit of trouble. Also, the debugger can get
    into a bad state occasionally and eclipse will need to be restarted.
*   Refactoring seems to work in some instances, but be afraid of refactors that
    touch a lot of files.

[TOC]

## Setup

### Get & Configure Eclipse

Eclipse 4.6.1 (Neon) is known to work with Chromium for Linux.

*   [Download](http://www.eclipse.org/downloads/) the distribution appropriate
    for your OS. For example, for Linux 64-bit/Java 64-bit, use the Linux 64 bit
    package (Eclipse Packages Tab -> Linux 64 bit (link in bottom right)).
    *   Tip: The packaged version of eclipse in distros may not work correctly
        with the latest CDT plugin (installed below). Best to get them all from
        the same source.
    *   Googlers: The version installed on Goobuntu works fine. The UI will be
        much more responsive if you do not install the google3 plug-ins. Just
        uncheck all the boxes at first launch.
*   Unpack the distribution and edit the eclipse/eclipse.ini to increase the
    heap available to java. For instance:
    *   Change `-Xms40m` to `-Xms1024m` (minimum heap) and `-Xmx256m` to
        `-Xmx3072m` (maximum heap).
    *   Googlers: Edit `~/.eclipse/init.sh` to add this:

```
export ECLIPSE_MEM_START="1024M"
export ECLIPSE_MEM_MAX="3072M"
```

The large heap size prevents out of memory errors if you include many Chrome
subprojects that Eclipse is maintaining code indices for.

*   Turn off Hyperlink detection in the Eclipse preferences. (Window ->
    Preferences, search for "Hyperlinking, and uncheck "Enable on demand
    hyperlink style navigation").

Pressing the control key on (for keyboard shortcuts such as copy/paste) can
trigger the hyperlink detector. This occurs on the UI thread and can result in
the reading of jar files on the Eclipse classpath, which can tie up the editor
due to the size of the classpath in Chromium.

### A short word about paths

Before you start setting up your work space - here are a few hints:

*   Don't put your checkout on a remote file system (e.g. NFS filer). It's too
    slow both for building and for Eclipse.
*   Make sure there is no file system link in your source path because Ninja
    will resolve it for a faster build and Eclipse / GDB will get confused.
    (Note: This means that the source will possibly not reside in your user
    directory since it would require a link from filer to your local
    repository.)

### Run Eclipse & Set your workspace

Run eclipse/eclipse in a way that your regular build environment (export CC,
CXX, etc...) will be visible to the eclipse process.

Set the Workspace to be a directory on a local disk (e.g.
`/work/workspaces/chrome`).  Placing it on an NFS share is not recommended --
it's too slow and Eclipse will block on access.  Don't put the workspace in the
same directory as your checkout.

### Install the C Development Tools ("CDT")

1.  From the Help menu, select Install New Software...
    1.  Select the 'Work with' URL for the CDT
        If it's not there you can click Add... and add it.
        See https://eclipse.org/cdt/downloads.php for up to date versions,
        e.g. with CDT 8.7.0 for Eclipse Mars, use
        http://download.eclipse.org/tools/cdt/releases/8.7
    1.  Googlers: We have a local mirror, but be sure you run prodaccess before
        trying to use it.
1.  Select & install the Main and Optional features.
1.  Restart Eclipse
1.  Go to Window > Open Perspective > Other... > C/C++ to switch to the C++
    perspective (window layout).
1.  Right-click on the "Java" perspective in the top-right corner and select
    "Close" to remove it.

### Create your project(s)

First, turn off automatic workspace refresh and automatic building, as Eclipse
tries to do these too often and gets confused:

1.  Open Window > Preferences
1.  Search for "workspace"
1.  Turn off "Build automatically"
1.  Turn off "Refresh using native hooks or polling"
1.  Click "Apply"

Create a single Eclipse project for everything:

1.  From the File menu, select New > Project...
1.  Select C/C++ Project > Makefile Project with Existing Code
1.  Name the project the exact name of the directory: "src" (or "WebKit" if you
    mainly work in Blink and want a faster experience)
1.  Provide a path to the code, like /work/chromium/src (or
    /work/chromium/src/third_party/WebKit)
1.  Select toolchain: Linux GCC
1.  Click Finish.

Chromium uses C++14, so tell the indexer about it. Otherwise it will get
confused about things like std::unique_ptr.

1.  Right-click on "src" and select "Properties..."
1.  Navigate to C/C++ General > Preprocess Include Paths, Macros etc. >
    Providers
1.  Select CDT GCC Built-in Compiler Settings
1.  In the text box entitled Command to get compiler specs append "-std=c++14"
    (leaving out the quotes)

Chromium has a huge amount of code, enough that Eclipse can take a very long
time to perform operations like "go to definition" and "open resource". You need
to set it up to operate on a subset of the code.

In the Project Explorer on the left side:

1.  Right-click on "src" and select "Properties..."
1.  Open Resource > Resource Filters
1.  Click "Add Filter..."
1.  Add the following filter:
    *   Include only
    *   Files, all children (recursive)
    *   Name matches
        `.*\.(c|cc|cpp|h|mm|inl|idl|js|json|css|html|gyp|gypi|grd|grdp|gn|gni|mojom)`
        regular expression
1.  Add another filter:
    *   Exclude all
    *   Folders
    *   Name matches `out_.*|\.git|web_tests` regular expression
        *   If you aren't working on WebKit, adding `|blink` will remove more
            files
1.  Click "Apply and Close"

Don't exclude the primary "out" directory, as it contains generated header files
for things like string resources and Eclipse will miss a lot of symbols if you
do.

Eclipse will refresh the workspace and start indexing your code. It won't find
most header files, however. Give it more help finding them:

1.  Open Window > Preferences
1.  Search for "Indexer"
1.  Turn on "Allow heuristic resolution of includes"
1.  Select "Use active build configuration"
1.  Set Cache limits > Index database > Limit relative... to 20%
1.  Set Cache limits > Index database > Absolute limit to 256 MB
1.  Click "Apply and Close"

Now the indexer will find many more include files, regardless of which approach
you take below.

Eclipse will still complain about unresolved includes or invalid declarations
(semantic errors or code analysis errors in the ```Problems``` tab),
which you can set eclipse to ignore:

1.  Right-click on "src" and select "Properties..."
    * Open C++ General > Code Analysis
    * Change the severity from ```Error``` to ```Warning``` for each of the
    settings that you want eclipse to ignore.

#### Optional: Manual header paths and symbols

You can manually tell Eclipse where to find header files, which will allow it to
create the source code index before you do a real build.

1.  Right-click on "src" and select "Properties..."
    * Open C++ General > Paths and Symbols > Includes
    * Click "GNU C++"
    * Click "Add..."
    * Add `/path/to/chromium/src`
    * Check "Add to all configurations" and "Add to all languages"
1.  Repeat the above for:
    * `/path/to/chromium/src/testing/gtest/include`

You may also find it helpful to define some symbols.

1.  Add `OS_LINUX`:
    * Select the "Symbols" tab
    * Click "GNU C++"
    * Click "Add..."
    * Add name `OS_LINUX` with value 1
    * Click "Add to all configurations" and "Add to all languages"
1.  Repeat for `ENABLE_EXTENSIONS 1`
1.  Repeat for `HAS_OUT_OF_PROC_TEST_RUNNER 1`
1.  Click "OK".
1.  Eclipse will ask if you want to rebuild the index. Click "Yes".

Let the C++ indexer run.  It will take a while (10s of minutes).

### Optional: Building inside Eclipse

This allows Eclipse to automatically discover include directories and symbols.
If you use gold or ninja (both recommended) you'll need to tell Eclipse about
your path.

1.  echo $PATH from a shell and copy it to the clipboard
1.  Open Window > Preferences > C/C++ > Build > Environment
1.  Select "Replace native environment with specified one" (since gold and ninja
    must be at the start of your path)
1.  Click "Add..."
1.  For name, enter `PATH`
1.  For value, paste in your path with the ninja and gold directories.
1.  Click "OK"

To create a Make target:

1.  From the Window menu, select Show View > Make Target
1.  In the Make Target view, right-click on the project and select New...
1.  name the target (e.g. base\_unittests)
1.  Unclick the Build Command: Use builder Settings and type whatever build
    command you would use to build this target (e.g.
   `ninja -C out/Debug base_unittests`).
1.  Return to the project properties page a under the C/C++ Build, change the
    Build Location/Build Directory to be /path/to/chromium/src
    1.  In theory `${workspace_loc}` should work, but it doesn't for me.
    1.  If you put your workspace in `/path/to/chromium`, then
        `${workspace_loc:/src}` will work too.
1.  Now in the Make Targets view, select the target and click the hammer icon
    (Build Make Target).

You should see the build proceeding in the Console View and errors will be
parsed and appear in the Problems View. (Note that sometimes multi-line compiler
errors only show up partially in the Problems view and you'll want to look at
the full error in the Console).

(Eclipse 3.8 has a bug where the console scrolls too slowly if you're doing a
fast build, e.g. with reclient.  To work around, go to Window > Preferences and
search for "console".  Under C/C++ console, set "Limit console output" to
2147483647, the maximum value.)

### Optional: Multiple build targets

If you want to build multiple different targets in Eclipse (`chrome`,
`unit_tests`, etc.):

1.  Window > Show Toolbar (if you had it off)
1.  Turn on special toolbar menu item (hammer) or menu bar item (Project > Build
    configurations > Set Active > ...)
    1.  Window > Customize Perspective... > "Command Groups Availability"
    1.  Check "Build configuration"
1.  Add more Build targets
    1.  Project > Properties > C/C++ Build > Manage Configurations
    1.  Select "New..."
    1.  Duplicate from current and give it a name like "Unit tests".
    1.  Change under “Behavior” > Build > the target to e.g. `unit_tests`.

You can also drag the toolbar to the bottom of your window to save vertical
space.

### Optional: Running inside eclipse

Running inside eclipse is fairly straightforward:

1. Create a ```C/C++ Application```:
     1. ```Run``` > ```Run configurations```
     2. Double click on ```C/C++ Application```
     3. Pick a  name (e.g. ```shell```)
     4. Point to ```C/C++ Application```
        (e.g. ```src/out/Default/content_shell```)
     6. Click ```Debug``` to run the program.

### Optional: Debugging

1.  From the toolbar at the top, click the arrow next to the debug icon and
    select Debug Configurations...
1.  Select C/C++ Application and click the New Launch Configuration icon. This
    will create a new run/debug con figuration under the C/C++ Application header.
1.  Name it something useful (e.g. `base_unittests`).
1.  Under the Main Tab, enter the path to the executable (e.g.
    `.../out/Debug/base_unittests`)
1.  Select the Debugger Tab, select Debugger: gdb and unclick "Stop on startup
    in (main)" unless you want this.
1.  Set a breakpoint somewhere in your code and click the debug icon to start
    debugging.

#### Multi-process debugging

If you set breakpoints and your debugger session doesn't stop it is because
both ```chrome``` and ```content_shell ``` spawn sub-processes.
To debug, you need to attach a debugger to one of those sub-processes.

Eclipse can attach automatically to forked processes
(Run -> Debug configurations -> Debugger tab), but that doesn't seem to
work well.

The overall idea is described [here](https://www.chromium.org/blink/getting-started-with-blink-debugging)
, but one way to accomplish this in eclipse is to run two ```Debug configurations```:

1. Create a ```C/C++ Application```:
     1. ```Run``` > ```Debug configurations```
     2. Double click on ```C/C++ Application```
     3. Pick a  name (e.g. ```shell```)
     4. Point to ```C/C++ Application```
        (e.g. ```src/out/Default/content_shell```)
     5. In the arguments tab, add the following the to program arguments:
        ```--no-sandbox --renderer-startup-dialog test.html```
     6. Click ```Debug``` to run the program.
     7. That will run the application and it will stop with a message like the
         following:
       ```Renderer (239930) paused waiting for debugger to attach. Send SIGUSR1 to unpause.```
     9. ```239930``` is the number of the process running waiting for the ```signal```.
2. Create a ```C/C++ Attach to Application```:
    1. ```Run``` > ```Debug configurations```
    2. Double click on ```C/C++ Attach to Application```
    3. Pick  a name (e.g. ```shell proc```)
    4. Click ```Debug``` to run the configuration.
    5. In the ```Select Processes``` dialog, pick the process that was
        spawned above (if you type ```content_shell``` it will filter by
        name)
    6. Click on ```Debugger console``` to access the ```gdb``` console.
    7. Send the original process a signal
        ```signal SIGUSR1```
    8. That should unblock the original process and you should now be able to
        set breakpoints.

### Optional: Accurate symbol information

If setup properly, Eclipse can do a great job of semantic navigation of C++ code
(showing type hierarchies, finding all references to a particular method even
when other classes have methods of the same name, etc.). But doing this well
requires the Eclipse knows correct include paths and pre-processor definitions.
After fighting with with a number of approaches, I've found the below to work
best for me.

1.  From a shell in your src directory, run
    `gn gen --ide=eclipse out/Debug/` (replacing Debug with the output directory you normally use when building).
    1.  This generates <project root>/out/Debug/eclipse-cdt-settings.xml which
        is used below.
    1.  This creates a single list of include directories and preprocessor
        definitions to be used for all source files, and so is a little
        inaccurate. Here are some tips for compensating for the limitations:
        1.  If you care about blink, move 'third\_party/WebKit/Source' to the
            top of the list to better resolve ambiguous include paths (eg.
            `config.h`).
1.  Import paths and symbols
    1.  Right click on the project and select Properties > C/C++ General > Paths
        and Symbols
    1.  Click Restore Defaults to clear any old settings
    1.  Click Import Settings... > Browse... and select
        `<project root>/out/Debug/eclipse-cdt-settings.xml`
    1.  Click the Finish button.  The entire preferences dialog should go away.
    1.  Right click on the project and select Index > Rebuild

### Alternative: Per-file accurate include/pre-processor information

Instead of generating a fixed list of include paths and pre-processor
definitions for a project (above), it is also possible to have Eclipse determine
the correct setting on a file-by-file basis using a built output parser. I
(rbyers) used this successfully for a long time, but it doesn't seem much better
in practice than the simpler (and less bug-prone) approach above.

1.  Install the latest version of Eclipse IDE for C/C++ developers
    ([Juno SR1](http://www.eclipse.org/downloads/packages/eclipse-ide-cc-developers/junosr1)
    at the time of this writing)
1.  Setup build to generate a build log that includes the g++ command lines for
    the files you want to index:
    1.  Project Properties -> C/C++ Build
        1.  Uncheck "Use default build command"
        1.  Enter your build command, eg: `ninja -v`
            1.  Note that for better performance, you can use a command that
                doesn't actually builds, just prints the commands that would be
                run. For ninja/make this means adding -n. This only prints the
                compile commands for changed files (so be sure to move your
                existing out directory out of the way temporarily to force a
                full "build"). ninja also supports "-t commands" which will
                print all build commands for the specified target and runs even
                faster as it doesn't have to check file timestamps.
        1.  Build directory: your build path including out/Debug
            1.  Note that for the relative paths to be parsed correctly you
                can't use ninja's `-C <dir>` to change directories as you might
                from the command line.
        1.  Build: potentially change `all` to the target you want to analyze,
            eg. `chrome`
        1.  Deselect 'clean'
    1.  If you're using Ninja, you need to teach eclipse to ignore the prefix it
        adds (eg. `[10/1234]` to each line in build output):
        1.  Project properties -> C/C++ General -> Preprocessor includes
        1.  Providers -> CDT GCC Build Output Parser -> Compiler command pattern
        1.  `(\[.*\] )?((gcc)|([gc]\+\+)|(clang(\+\+)?))`
        1.  Note that there appears to be a bug with "Share setting entries
            between projects" - it will keep resetting to off. I suggest using
            per-project settings and using the "folder" as the container to keep
            discovered entries ("file" may work as well).
    1.  Eclipse / GTK has bugs where lots of output to the build console can
        slow down the UI dramatically and cause it to hang (basically spends all
        it's time trying to position the cursor correctly in the build console
        window). To avoid this, close the console window and disable
        automatically opening it on build:
        1.  Preferences->C/C++->Build->Console -> Uncheck "Open console when
            building"
        1.  note you can still see the build log in
            `<workspace>/.metadata/.plugins/org.eclipse.cdt.ui`
1.  Now build the project (select project, click on hammer).  If all went well:
    1.  Right click on a cpp file -> properties -> C/C++ general -> Preprocessor
        includes -> GNU C++ -> CDT GCC Build output Parser
    1.  You will be able to expand and see all the include paths and
        pre-processor definitions used for this file
1.  Rebuild index (right-click on project, index, rebuild). If all went well:
    1.  Open a CPP file and look at problems windows
    1.  Should be no (or very few) errors
    1.  Should be able to hit F3 on most symbols and jump to their definitioin
    1.  CDT has some issues with complex C++ syntax like templates (eg.
        `PassOwnPtr` functions)
    1.  See
        [this page](http://wiki.eclipse.org/CDT/User/FAQ#Why_does_Open_Declaration_.28F3.29_not_work.3F_.28also_applies_to_other_functions_using_the_indexer.29)
        for more information.

### Optional: static code and style guide analysis using cpplint.py

1.  From the toolbar at the top, click the Project -> Properties and go to
    C/C++Build.
    1.  Click on the right side of the pop up windows, "Manage
        Configurations...", then on New, and give it a name, f.i. "Lint current
        file", and close the small window, then select it in the Configuration
        drop down list.
    1.  Under Builder settings tab, unclick "Use default build command" and type
        as build command the full path to your `depot_tools/cpplint.py`
    1.  Under behaviour tab, unselect Clean, select Build(incremental build) and
        in Make build target, add `--verbose=0 ${selected_resource_loc}`
    1.  Go back to the left side of the current window, and to C/C++Build ->
        Settings, and click on error parsers tab, make sure CDT GNU C/C++ Error
        Parser, CDT pushd/popd CWD Locator are set, then click Apply and OK.
1.  Select a file and click on the hammer icon drop down triangle next to it,
    and make sure the build configuration is selected "Lint current file", then
    click on the hammer.
1.  Note: If you get the `cpplint.py help` output, make sure you have selected a
    file, by clicking inside the editor window or on its tab header, and make
    sure the editor is not maximized inside Eclipse, i.e. you should see more
    subwindows around.

### Additional tips

1.  Mozilla's
    [Eclipse CDT guide](https://developer.mozilla.org/en-US/docs/Eclipse_CDT)
    is helpful:
1.  For improved performance, I use medium-granularity projects (eg. one for
    WebKit/Source) instead of putting all of 'src/' in one project.
1. Running [```content_shell```](https://www.chromium.org/developers/content-module)
   as opposed to all of ```chrome```  is a lot faster/smaller.
