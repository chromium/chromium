*Web Engine Shell*

The Web Engine Shell is a simple command-line executable which will create a
fullscreen browsing session and navigate it to a specified URL. It can be used
for validating web platform features, or it can serve as example code for
embedding Web Engine in C++ applications.

**Usage**
To build and run Web Engine Shell, execute the following commands:

```
$ autoninja -C $OUTDIR web_engine_shell
$ $OUTDIR/bin/install_web_engine_shell --fuchsia-out-dir $FUCHSIA_OUTDIR
$ cd $FUCHSIA
$ fx shell
$ run fuchsia-pkg://fuchsia.com/web_engine_shell#meta/web_engine_shell.cmx --remote-debugging-port=1234 http://www.example.com
```

Local files can be deployed with the Web Engine Shell and accessed via the
URL `fuchsia-dir://shell-data/PATH/TO/FILE`. Files may be added to the directory
by placing them under the path `//fuchsia/engine/test/shell_data`.

Here is an example command line which loads a local file:
```
$ run fuchsia-pkg://fuchsia.com/web_engine_shell#meta/web_engine_shell.cmx fuchsia-dir://shell-data/index.html
```
