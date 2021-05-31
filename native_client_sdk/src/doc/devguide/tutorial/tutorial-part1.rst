.. _tutorial:

.. include:: /migration/deprecation.inc

######################################
C++ Tutorial: Getting Started (Part 1)
######################################

.. contents::
  :local:
  :backlinks: none
  :depth: 2

Overview
========

This tutorial shows how to build and run a web application using Portable Native
Client (PNaCl). This is a client-side application that uses HTML, JavaScript and
a Native Client module written in C++. The PNaCl toolchain is used to enable
running the Native Client module directly from a web page.

It's recommended that you read the :doc:`Native Client Technical Overview
</overview>` prior to going through this tutorial.

What the application in this tutorial does
------------------------------------------

The application in this tutorial shows how to load a Native Client module in a
web page, and how to send messages between JavaScript and the Native Client 
module. In this simple application, the JavaScript sends a ``'hello'`` message 
to the Native Client module. When the Native Client module receives a message, 
it checks whether the message is equal to the string ``'hello'``. If it is, the
Native Client module returns a message saying ``'hello from NaCl'``. A 
JavaScript alert panel displays the message received from the Native Client 
module.

Communication between JavaScript and Native Client modules
----------------------------------------------------------

The Native Client programming model supports bidirectional communication between
JavaScript and the Native Client module. Both sides can initiate
and respond to messages. In all cases, the communication is asynchronous: The
caller (JavaScript or the Native Client module) sends a message, but the caller
does not wait for, or may not even expect, a response. This behavior is
analogous to client/server communication on the web, where the client posts a
message to the server and returns immediately. The Native Client messaging
system is part of the Pepper API, and is described in detail in
:doc:`Developer's Guide: Messaging System </devguide/coding/message-system>`.
It is also similar to the way `web workers
<http://en.wikipedia.org/wiki/Web_worker>`_ interact with the main document in
JavaScript.

Step 1: Download and install the Native Client SDK
==================================================

Follow the instructions on the :doc:`Download </sdk/download>` page to
download and install the Native Client SDK.

.. _tutorial_step_2:

Step 2: Start a local server
============================

To simulate a production environment, the SDK provides a simple web server that
can be used to serve the application on ``localhost``. A convenience Makefile
rule called ``serve`` is the easiest way to invoke it:

.. naclcode::
  :prettyprint: 0

  $ cd pepper_$(VERSION)/getting_started
  $ make serve

.. Note::
  :class: note

  The SDK may consist of several "bundles", one per Chrome/Pepper version (see
  :doc:`versioning information </version>`). In the sample invocation above
  ``pepper_$(VERSION)`` refers to the specific version you want to use. For
  example, ``pepper_37``. If you don't know which version you need, use the
  one labeled ``(stable)`` by the ``naclsdk list`` command. See 
  :doc:`Download the Native Client SDK </sdk/download>` for more details.

If no port number is specified, the server defaults to port 5103, and can be
accessed at ``http://localhost:5103``.

Any server can be used for the purpose of development. The one provided with the
SDK is just a convenience, not a requirement.

.. _tutorial_step_3:

Step 3: Set up the Chrome browser
=================================

PNaCl is enabled by default in Chrome. We recommend that you use a version of
Chrome that's the same or newer than the SDK bundle used to build Native Client
modules. Older PNaCl modules will always work with newer versions of Chrome, but
the converse is not true.

.. Note::
  :class: note

  To find out the version of Chrome, type ``about:chrome`` in the address bar.

For a better development experience, it's also recommended to disable the
Chrome cache. Chrome caches resources aggressively; disabling the cache helps
make sure that the latest version of the Native Client module is loaded during
development.

* Open Chrome's developer tools by clicking the menu icon |menu-icon| and
  choosing ``Tools > Developer tools``.
* Click the gear icon |gear-icon| in the bottom right corner of the Chrome
  window.
* Under the "General" settings, check the box next to "Disable cache (while
  DevTools is open)".
* Keep the Developer Tools pane open while developing Native Client
  applications.

.. |menu-icon| image:: /images/menu-icon.png
.. |gear-icon| image:: /images/gear-icon.png

Step 4: Stub code for the tutorial
==================================

The stub code for the tutorial is available in the SDK, in
``pepper_$(VERSION)/getting_started/part1``. It contains the following files:

* ``index.html``: Contains the HTML layout of the page as well as the JavaScript
  code that interacts with the Native Client module.

  The Native Client module is included in the page with an ``<embed>`` tag that
  points to a manifest file.
* ``hello_tutorial.nmf``: A manifest file that's used to point the HTML to the
  Native Client module and optionally provide additional commands to the PNaCl
  translator that is part of the Chrome browser.
* ``hello_tutorial.cc``: C++ code for a simple Native Client module.
* ``Makefile``: Compilation commands to build the **pexe** (portable executable)
  from the C++ code in ``hello_tutorial.cc``.

It's a good idea to take a look at these files now---they contain a large amount
of comments that help explain their structure and contents. For more details
on the structure of a typical Native Client application, see
:doc:`Application Structure </devguide/coding/application-structure>`.

The stub code is intentionally very minimal. The C++ code does not do anything
except correctly initialize itself. The JavaScript code waits for the Native
Client module to load and changes the status text on the web page accordingly.

.. _tutorial_step_5:

Step 5: Compile the Native Client module and run the stub application
=====================================================================

To compile the Native Client module, run ``make``:

.. naclcode::
  :prettyprint: 0

  $ cd pepper_$(VERSION)/getting_started/part1
  $ make

Since the sample is located within the SDK tree, the Makefile knows how to find
the PNaCl toolchain automatically and use it to build the module. If you're
building applications outside the NaCl SDK tree, you should set the
``$NACL_SDK_ROOT`` environment variable. See :doc:`Building Native Client
Modules </devguide//devcycle/building>` for more details.

Assuming the local server was started according to the instructions in
:ref:`Step 2 <tutorial_step_2>`, you can now load the sample by pointing Chrome
to ``http://localhost:5103/part1``. Chrome should load the Native Client module
successfully and the Status text should change from "LOADING..." to "SUCCESS".
If you run into problems, check out the :ref:`Troubleshooting section
<tutorial_troubleshooting>` below.

Step 6: Modify the JavaScript code to send a message to the Native Client module
================================================================================

In this step, you'll modify the web page (``index.html``) to send a message to
the Native Client module after the page loads the module.

Look for the JavaScript function ``moduleDidLoad()``, and add new code to send
a 'hello' message to the module. The new function should look as follows:

.. naclcode::

    function moduleDidLoad() {
      HelloTutorialModule = document.getElementById('hello_tutorial');
      updateStatus('SUCCESS');
      // Send a message to the Native Client module
      HelloTutorialModule.postMessage('hello');
    }

Step 7: Implement a message handler in the Native Client module
===============================================================

In this step, you'll modify the Native Client module (``hello_tutorial.cc``) to
respond to the message received from the JavaScript code in the application.
Specifically, you'll:

* Implement the ``HandleMessage()`` member function of the module instance.
* Use the ``PostMessage()`` member function to send a message from the module to
  the JavaScript code.

First, add code to define the variables used by the Native Client module (the
'hello' string you're expecting to receive from JavaScript and the reply string
you want to return to JavaScript as a response). In the file
``hello_tutorial.cc``, add this code after the ``#include`` statements:

.. naclcode::

  namespace {
  // The expected string sent by the browser.
  const char* const kHelloString = "hello";
  // The string sent back to the browser upon receipt of a message
  // containing "hello".
  const char* const kReplyString = "hello from NaCl";
  } // namespace

Now, implement the ``HandleMessage()`` member function to check for
``kHelloString`` and return ``kReplyString.`` Look for the following line:

.. naclcode::

    // TODO(sdk_user): 1. Make this function handle the incoming message.

Populate the member function with code, as follows:

.. naclcode::

  virtual void HandleMessage(const pp::Var& var_message) {
    if (!var_message.is_string())
      return;
    std::string message = var_message.AsString();
    pp::Var var_reply;
    if (message == kHelloString) {
      var_reply = pp::Var(kReplyString);
      PostMessage(var_reply);
    }
  }

See the Pepper API documentation for additional information about the
`pp::Instance.HandleMessage
</native-client/pepper_stable/cpp/classpp_1_1_instance.html#a5dce8c8b36b1df7cfcc12e42397a35e8>`_
and `pp::Instance.PostMessage
</native-client/pepper_stable/cpp/classpp_1_1_instance.html#a67e888a4e4e23effe7a09625e73ecae9>`_
member functions.

Step 8: Compile the Native Client module and run the application again
======================================================================

#. Compile the Native Client module by running the ``make`` command again.
#. Start the SDK web server by running ``make server``.
#. Re-run the application by reloading ``http://localhost:5103/part1`` in 
   Chrome.
   
   After Chrome loads the Native Client module, you should see the message sent
   from the module.

.. _tutorial_troubleshooting:

Troubleshooting
===============

If your application doesn't run, see :ref:`Step 3 <tutorial_step_3>` above to
verify that you've set up your environment correctly, including both the Chrome
browser and the local server. Make sure that you're running a correct version of
Chrome, which is also greater or equal than the SDK bundle version you are
using.

Another useful debugging aid is the Chrome JavaScript console (available via the
``Tools`` menu in Chrome). Examine it for clues about what went wrong. For
example, if there's a message saying "NaCl module crashed", there is a
possibility that the Native Client module has a bug; :doc:`debugging
</devguide/devcycle/debugging>` may be required.

There's more information about troubleshooting in the documentation:

* :ref:`FAQ Troubleshooting <faq_troubleshooting>`.
* The :doc:`Progress Events </devguide/coding/progress-events>` document
  contains some useful information about handling error events.

Next steps
==========

* See the :doc:`Application Structure </devguide/coding/application-structure>`
  section in the Developer's Guide for information about how to structure a
  Native Client module.
* Check the `C++ Reference </native-client/pepper_stable/cpp>`_ for details
  about how to use the Pepper APIs.
* Browse through the source code of the SDK examples (in the ``examples``
  directory) to learn additional techniques for writing Native Client
  applications and using the Pepper APIs.
* See the :doc:`Building </devguide/devcycle/building>`, :doc:`Running
  </devguide/devcycle/running>`, and :doc:`Debugging pages
  </devguide/devcycle/debugging>` for information about how to build, run, and
  debug Native Client applications.
* Check the `webports <https://chromium.googlesource.com/webports>`_ project to
  see what libraries have been ported for use with Native Client. If you port an
  open-source library for your own use, we recommend adding it to webports
  (see `How to check code into webports
  <https://chromium.googlesource.com/webports/+/main/CONTRIBUTING.md>`_).
