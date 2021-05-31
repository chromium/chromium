.. _cds2014_python:

.. include:: /migration/deprecation.inc

########################################
What a Difference Python Makes - Codelab
########################################

Introduction
------------

.. include:: python_summary.inc

.. include:: ../nacldev/setup_web.inc


Get the Code!
-------------

Rather than start from nothing, for this codelab we've provided
you with a zip file containing a starting point.

Download the codelab::

  geturl https://nacltools.storage.googleapis.com/cds2014/cds2014_python.zip cds2014_python.zip

Unzip it::

  unzip cds2014_python.zip

Go into the codelab directory::

  cd cds2014_python

Create a new local git repo::

  git init

Add everything::

  git add .

Commit it::

  git commit -am "initial"

While working, you can see what you've changed by running::

  git diff


Your challenge, should you choose to accept it...
-------------------------------------------------

Javascript has many wonderful features out of the box.
Unfortunately, generating textual diffs is not one of them.
Python on the other hand has the |difflib| module in its standard library.

The starting point you've just extracted contains the shell
of a web app using Portable Native Client Python to generate a diff.
Just one thing is missing, that whole diffing thing...

To see where things stand, deploy the sample like this::

  make

This will attempt to open the sample, but will be blocked by
a popup blocker the first time. Click on the URL to accept the popup.
It also clobbers /tmp/differ for good measure.

As you can see, this isn't quite what we're going for.

You'll want to modify diff.py, using the editor you selected earlier.
You'll probably want to consult the |difflib| documentation.
I would suggest you check out the HtmlDiff class and make use of
the make_file member function. Note our goal is to create a
full HTML diff, so the make_table function is insufficient.
The splitlines function may also come in handy.

You can test diff.py manually as you would in a normal UNIX environment.
It can be run like this::

  ./diff.py before.txt after.txt out.html

Once everything is working, diff.html will contain an html diff.
After the initial `make` you can hit "Run" to test your changes.

Now get to it, and good luck!


What you've learned
-------------------

While the details of building and packaging Python have been
insulated from you for the purpose of this exercise, the key take-home lesson
is how easy it is to leverage Python using PNaCl.
As you've likely experienced, the initial start time is non-trivial.
We're working on improving this, both by improving PNaCl,
and our Python port.

The same approach to deploying Python apps can be used for the other
interpreted languages that have been ported to PNaCl.

Check out the range of interpreters, libraries, and tools
`already ported to PNaCl and ready to be integrated with your Web App
<https://chromium.googlesource.com/webports/+/main/docs/port_list.md>`_.

While our in-browser environment is rapidly evolving
to become a complete development solution,
for the broadest range of development options, check out the
`NaCl SDK
<https://developer.chrome.com/native-client/sdk/download>`_.

Send us comments and feedback on the `native-client-discuss
<https://groups.google.com/forum/#!forum/native-client-discuss>`_ mailing list,
or ask questions using Stack Overflow's `google-nativeclient
<https://stackoverflow.com/questions/tagged/google-nativeclient>`_ tag.

Bring your interpreted app to PNaCl today!

.. include:: ../nacldev/cleanup_web.inc
