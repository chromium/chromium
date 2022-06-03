# Property classes

This directory contains implementations for CSS property classes, as well as
Utils files containing functions commonly used by the property classes.

[TOC]

## Overview

Property class inheritance structure:

![Property classes](https://image.ibb.co/i6P7HG/Property_class_inheritance.png)

CSSUnresolvedProperty is the base class which all property implementations are
derived from, either directly or indirectly. This class includes logic that
needs to be implemented by alias classes (a small subset of property logic), as
well as static methods to get the property class for every property. Alias
classes inherit directly from CSSUnresolvedProperty.

CSSProperty is another base class, which inherits from CSSUnresolvedProperty.
It contains all the methods that can be called on a resolved property class, and
a default implementation for each. The implementation for CSSProperty is
partially generated (css_property.h and css_property.cc) and partially hand
written (CSSPropertyBaseCustom.cpp).

Longhand and Shorthand are base classes which inherit from CSSProperty, from
which all longhand and shorthand property classes directly derive. Variable is a
special property class that represents custom properties, which inherits from
CSSProperty.

The methods that are overriden from the base classes by a property class depends
on the functionality required by that property. Methods may have a working
default implementation in the base classes, or may assert that this default
implementation is not reached.

Alias classes, longhand classes, and shorthand classes each represent a single
CSS property and are named after that property. These property classes are
partially generated (<PropertyName\>.h and for some properties
<PropertyName\>.cpp), and partially hand written (in shorthands_custom.cc or
longhands_custom.cc).


## Special property classes

### Aliases

Aliases are properties that share most of their logic with another property,
sometimes with the exception of some minor differences in parsing logic due to
legacy reasons. Many aliases are -webkit prefixed properties that have since
been implemented without the prefix. Aliases define the alias_for member in
[css_properties.json5](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/css/css_properties.json5)).

Alias classes implement a small subset of functions of properties, only those
that do not share an implementation with the resolved property that they are an
alias for. Because of this, these classes inherit directly from a base class
CSSUnresolvedProperty.

### Shorthand properties

Shorthand properties only exist in the context of parsing and serialization.
Therefore only a subset of methods may be implemented by shorthand properties,
e.g. ParseShorthand.

### Descriptors

Descriptors define the characteristics of an at-rule. For example, @font-face
is an at-rule, and font-family is a valid descriptor for @font-face. Within the
context of @font-face, font-family names a web font associated with a url src
descriptor, but the font-family property is used to select a font among the
available local fonts or web fonts. From this example we can see that a
descriptor is not the same as a CSS property with the same name. Sometimes
descriptors and CSS properties with the same name are handled together, but
that should not be taken to mean that they are the same thing. Fixing this
possible source of confusion is an open issue crbug.com/752745.

### Variable

The Variable rule is not a true CSS property, but is treated
as such in places for convenience. It does not appear in css_properties.json5; thus
its property class header needed to be hand-written & manually added to the
list of property classes by make_css_property_base.py. Those hand-written
headers are in this directory.

## How to add a new property class

1.  Add a .cpp file to this directory named
    `<PropertyName>.cpp`
2.  Implement the property class in the .cpp file
    1.  Add `#include "core/css/properties/<longhands|shorthands>/<PropertyName>.h"`
        (this will be a generated file).
    2.  Implement the required methods on the property class.
3.  If logic is required by multiple property classes you may need to create a
    new Utils file. These utils methods are grouped by pipeline function (e.g.
    [css_parsing_utils](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/css/properties/css_parsing_utils.h)).
4.  Add the new property to `core/css/css_properties.json5`. Ensure that you
    include all the methods implemented on the property in the
    'property_methods' flag so that the header file is generated correctly (see
    [css_properties.json5](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/css/css_properties.json5)
    for more details)
5.  Add new files to BUILD files
    1.  Add the new .cpp file to
        [core/css/BUILD.gn](https://codesearch.chromium.org/chromium/src/third_party/blink/renderer/core/css/BUILD.gn)
        in the `blink_core_sources` target's `sources` parameter
    2.  Add the generated .h file to
        [core/BUILD.gn](https://codesearch.chromium.org/chromium/src/third_party/blink/renderer/core/BUILD.gn)
        in the `css_properties` target's `outputs` parameter. This step is often
        forgotten and may not immediately caues an error but can cause linking
        errors in certain situations.

See [this example CL](https://codereview.chromium.org/2735093005), which
converts the existing line-height property to use the CSSProperty design.
This new line-height property class only implements the ParseSingleValue method,
using shared logic through a utils file.

## How to use a property class

Property class instances may be accessed through the base class
CSSUnresolvedProperty, either by directly calling the get function for that
property
(CSSUnresolvedProperty::GetCSSProperty<PropertyName\>)(), or by passing the
CSSPropertyID to the general getter
(CSSUnresolvedProperty::Get(CSSPropertyID)).

Methods that handle property specific logic may be called directly on the
property class instance. Which methods are available depends on what type of
property it is (i.e. which base classes it implements).

## Status

Eventually, all logic pertaining to a single property will be found only
within its CSS property class where possible.

Currently (5 Jan 2018) the code base is in a transitional state and
property specific logic is still scattered around the code base. See Project
Ribbon
[tracking bug](https://bugs.chromium.org/p/chromium/issues/detail?id=545324) and
[design doc](https://docs.google.com/document/d/1ywjUTmnxF5FXlpUTuLpint0w4TdSsjJzdWJqmhNzlss/edit#heading=h.1ckibme4i78b)
for details of progress.
