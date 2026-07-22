# Symbol folder

This folder is supposed to contain the custom symbols, the definition of custom and system symbols and the code that is used to work with the symbols.

Symbol.h is the umbrella header for all the symbols and should be the one included.

When adding a new custom symbol, the name of the SVG file should have dot to separate words and end with ".cr.svg". The name of the symbolset should be the same as the file, but use underscore and remove the cr suffix.

The enum in symbol_enums.h is separated in the following sections:
* Branded symbols
* Custom symbols
* Default symbols

Each section should be ordered alphabetically.