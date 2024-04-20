`//pdf` contains the PDF plugin, its Blink-based replacement, as well as PDF
utility functions that leverage PDFium. It can use low-level components that
live below the content layer, as well as other foundational code like
`//printing`. It should not use `//content` or anything in `//components` that
lives above the content layer. Code that lives above the content layer should
live in `//components/pdf`, or in the embedder. All the code here should run in
sandboxed child processes.

TODO(crbug.com/40186598): Remove existing `//content` dependencies.
