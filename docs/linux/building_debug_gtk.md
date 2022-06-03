# Linux — Building and Debugging GTK

Sometimes installing the debug packages for gtk and glib isn't quite enough.
(For instance, if the artifacts from -O2 are driving you bonkers in gdb, you
might want to rebuild with -O0.)
Here's how to build from source and use your local version without installing
it.

[TOC]

## 32-bit systems

On Ubuntu, to download and build glib and gtk suitable for debugging:

1.  If you don't have a gpg key yet, generate one with `gpg --gen-key`.
2.  Create file `~/.devscripts` containing `DEBSIGN_KEYID=yourkey`, e.g.
    `DEBSIGN_KEYID=CC91A262` (See
    http://www.debian.org/doc/maint-guide/ch-build.en.html)
3.  If you're on a 32 bit system, do:

    ```shell
    #!/bin/sh
    set -x
    set -e
    # Workaround for "E: Build-dependencies for glib2.0 could not be satisfied"
    # See also https://bugs.launchpad.net/ubuntu/+source/apt/+bug/245068
    sudo apt-get install libgamin-dev
    sudo apt-get build-dep glib2.0 gtk+2.0
    rm -rf ~/mylibs
    mkdir ~/mylibs
    cd ~/mylibs
    apt-get source glib2.0 gtk+2.0
    cd glib2.0*
    DEB_BUILD_OPTIONS="nostrip noopt debug" debuild
    cd ../gtk+2.0*
    DEB_BUILD_OPTIONS="nostrip noopt debug" debuild
    ```

This should take about an hour. If it gets stuck waiting for a zombie,
you may have to kill its closest parent (the makefile uses subshells,
and bash seems to get confused). When I did this, it continued successfully.

At the very end, it will prompt you for the passphrase for your gpg key.

Then, to run an app with those libraries, do e.g.

    export LD_LIBRARY_PATH=$HOME/mylibs/gtk+2.0-2.16.1/debian/install/shared/usr/lib:$HOME/mylibs/gtk+2.0-2.20.1/debian/install/shared/usr/lib

gdb ignores that variable, so in the debugger, you would have to do something like

    set solib-search-path $HOME/mylibs/gtk+2.0-2.16.1/debian/install/shared/usr/lib:$HOME/mylibs/gtk+2.0-2.20.1/debian/install/shared/usr/lib

See also http://sources.redhat.com/gdb/current/onlinedocs/gdb_17.html

## 64-bit systems

If you're on a 64 bit system, you can do the above on a 32
bit system, and copy the result.  Or try one of the following:

### Building your own GTK

```shell
apt-get source glib-2.0 gtk+-2.0

export CFLAGS='-m32 -g'
export LDFLAGS=-L/usr/lib32
export LD_LIBRARY_PATH=/work/32/lib
export PKG_CONFIG_PATH=/work/32/lib/pkgconfig

# glib
setarch i386 ./configure --prefix=/work/32 --enable-debug=yes

# gtk
setarch i386 ./configure --prefix=/work/32 --enable-debug=yes --without-libtiff
```

### ia32-libs

_Note: Evan tried this and didn't get any debug libs at the end._

Or you could try this instead:

```
#!/bin/sh
set -x
set -e
sudo apt-get build-dep ia32-libs
rm -rf ~/mylibs
mkdir ~/mylibs
cd ~/mylibs
apt-get source ia32-libs
cd ia32-libs*
DEB_BUILD_OPTIONS="nostrip noopt debug" debuild
```

By default, this just grabs and unpacks prebuilt libraries; see
ia32-libs-2.7ubuntu6/fetch-and-build which documents a BUILD variable which
would force actual building. This would take way longer, since it builds dozens
of libraries. I haven't tried it yet.

#### Possible Issues

debuild may fail with

```
gpg: [stdin]: clearsign failed: secret key not available
debsign: gpg error occurred!  Aborting....
```

if you forget to create `~/.devscripts` with the right contents.

The build may fail with a `FAIL: abicheck.sh` if gold is your system linker. Use
ld instead.
