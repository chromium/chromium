# Web IDL compiler (web_idl package)

[TOC]

## What's web_idl?

Python package
[`web_idl`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/web_idl/)
is the core part of Web IDL compiler of Blink.
[`build_web_idl_database.py`](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/bindings/scripts/build_web_idl_database.py)
is the driver script, which takes a set of ASTs of *.idl files as inputs and
produces a database(-ish) pickle file that contains all information about
spec-author defined types (IDL interface, IDL dictionary, etc.).  The database
file will be used as the input of
[Blink-V8 bindings code generator](../bind_gen/README.md).

## Quick walk in web_idl_database.pickle

The following snippet of shell commands and Python scripts demonstrates how you
can build and use the database file (`web_idl_database.pickle`) and what you
can do with the database file just as an example.

```shell
# Build the database file `web_idl_database.pickle`.
# output: out/Default/gen/third_party/blink/renderer/bindings/web_idl_database.pickle
$ autoninja -C out/Default web_idl_database

# Play with the produced database.
$ PYTHONPATH=third_party/blink/renderer/bindings/scripts:$PYTHONPATH python3
>>> import web_idl
>>> web_idl_database_path = 'out/Default/gen/third_party/blink/renderer/bindings/web_idl_database.pickle'
>>> web_idl_database = web_idl.Database.read_from_file(web_idl_database_path)

# Print all IDL attributes whose type is boolean (but not nullable boolean).
>>> for interface in web_idl_database.interfaces:
...   for attribute in interface.attributes:
...     if attribute.idl_type.is_boolean:
...       print("{}.{}".format(interface.identifier, attribute.identifier))

# Print all IDL dictionary members which are required members.
>>> for dictionary in web_idl_database.dictionaries:
...   for member in dictionary.own_members:
...     if member.is_required:
...       print("{}.{}".format(dictionary.identifier, member.identifier))

# Print API references of IDL dictionary class.
>>> help(web_idl.Dictionary)

# Print API references of a certain object (Window interface in this case).
>>> window = web_idl_database.find('Window')
>>> help(window)
```

## Design, code structure, flow of compilation sequences, etc.

TODO(yukishiino): Write this section about how the IDL compiler transforms the inputs into the database file.
