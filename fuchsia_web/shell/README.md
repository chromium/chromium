*WebEngine Shell*

The WebEngine Shell is a simple command-line executable which will create a
fullscreen browsing session and navigate it to a specified URL. It can be used
for validating web platform features, or it can serve as example code for
embedding WebEngine in C++ applications.

**Usage**
To build and run WebEngine Shell, execute the following commands:

```
$ autoninja -C $OUTDIR web_engine_shell
$ $OUTDIR/bin/deploy_web_engine_shell --fuchsia-out-dir $FUCHSIA_OUTDIR
$ cd $FUCHSIA
$ fx shell
$ run fuchsia-pkg://fuchsia.com/web_engine_shell#meta/web_engine_shell.cmx --remote-debugging-port=1234 http://www.example.com
```

Local files can be deployed with the WebEngine Shell and accessed via the
URL `fuchsia-dir://data/PATH/TO/FILE`. Files may be added to the directory
by placing them under the path `//fuchsia_web/webengine/data`.

Here is an example command line which loads a local file:
```
$ run fuchsia-pkg://fuchsia.com/web_engine_shell#meta/web_engine_shell.cmx fuchsia-dir://data/index.html
```
