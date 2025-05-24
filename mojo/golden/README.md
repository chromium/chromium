# Generated Mojo Binding Golden Files

In this directory, we generate bindings for a small set of example .mojom files
and check them in so that the diff is readily available at review time. These
checked-in generated files are referred to as golden files.

If a change is made that would alter the generated bindings, a presubmit check
will prompt the change author to regenerate the golden files and check in any
changes to them.

The file extension `.golden` is appended to the end of the filename of each
generated file to (1) signify to developers that these files are not meant to
be compiled or executed, and (2) to exclude these files from the normal
presubmit checks (e.g. line length, copyright year).
