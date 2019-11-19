// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#include "base/format_macros.h"
#include "base/stl_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "ios/chrome/browser/web/chrome_web_client.h"
#include "ios/chrome/browser/web/chrome_web_test.h"
#import "ios/web/public/test/web_js_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Unit tests for ios/chrome/browser/web/resources/autofill_controller.js
namespace {

// Structure for getting element by name using JavaScripts.
struct ElementByName {
  // The name of the element.
  const char* element_name;
  // The index in the elements that have |element_name|.
  const int index;
  // The option index if the element is a select, -1 otherwise.
  const int option_index;
};

NSString* GetDefaultMaxLengthString() {
  return @"524288";
}

NSNumber* GetDefaultMaxLength() {
  return @524288;
}

// Generates the JavaScript that gets an element by name.
NSString* GetElementByNameJavaScript(ElementByName element) {
  NSString* query =
      [NSString stringWithFormat:@"window.document.getElementsByName('%s')[%d]",
                                 element.element_name, element.index];
  if (element.option_index >= 0) {
    query =
        [query stringByAppendingFormat:@".options[%d]", element.option_index];
  }
  return query;
}

// Generates an array of JavaScripts that get each element in |elements| by
// name.
NSArray* GetElementsByNameJavaScripts(const ElementByName elements[],
                                      size_t elements_size) {
  NSMutableArray* array = [NSMutableArray array];
  for (size_t i = 0; i < elements_size; ++i) {
    NSString* query = GetElementByNameJavaScript(elements[i]);
    [array addObject:query];
  }
  return array;
}

// clang-format off
NSString* kHTMLForTestingElements = @"<html><body>"
    "<input type=hidden name='gl' value='us'>"
    "<form name='testform'>"
    "  <input type=hidden name='hl' value='en'>"
    "  <input type='text' name='firstname'>"
    "  <input type='text' name='lastname'>"
    "  <input type='email' name='email'>"
    "  <input type='tel' name='phone'>"
    "  <input type='url' autocomplete='off' name='blog'>"
    "  <input type='number' name='expected number of clicks'>"
    "  <input type='password' autocomplete='off' name='pwd'>"
    "  <input type='checkbox' name='vehicle' value='Bike'>"
    "  <input type='checkbox' name='vehicle' value='Car'>"
    "  <input type='checkbox' name='vehicle' value='Rocket'>"
    "  <input type='radio' name='boolean' value='true'>"
    "  <input type='radio' name='boolean' value='false'>"
    "  <input type='radio' name='boolean' value='other'>"
    "  <label>State:"
    "    <select name='state'>"
    "      <option value='CA'>CA</option>"
    "      <option value='MA'>MA</option>"
    "    </select>"
    "   </label>"
    "  <label>Course:"
    "    <select name='course'>"
    "     <optgroup label='8.01 Physics I: Classical Mechanics'>"
    "      <option value='8.01.1'>Lecture 01: Powers of Ten"
    "      <option value='8.01.2'>Lecture 02: 1D Kinematics"
    "      <option value='8.01.3'>Lecture 03: Vectors"
    "     <optgroup label='8.02 Electricity and Magnestism'>"
    "      <option value='8.02.1'>Lecture 01: What holds our world together?"
    "      <option value='8.02.2'>Lecture 02: Electric Field"
    "      <option value='8.02.3'>Lecture 03: Electric Flux"
    "    </select>"
    "  </label>"
    "  <label>Cars:"
    "    <select name='cars' multiple>"
    "      <option value='volvo'>Volvo</option>"
    "      <option value='saab'>Saab</option>"
    "      <option value='opel'>Opel</option>"
    "      <option value='audi'>Audi</option>"
    "    </select>"
    "   </label>"
    "  <input type='submit' name='submit' value='Submit'>"
    "</form>"
    "</body></html>";
// clang-format on

// A bit field mask to extract data from WebFormControlElement. They are from
// autofill_controller.js
enum ExtractMask {
  EXTRACT_NONE = 0,
  EXTRACT_VALUE = 1 << 0,        // Extract value from WebFormControlElement.
  EXTRACT_OPTION_TEXT = 1 << 1,  // Extract option text from
                                 // WebFormSelectElement. Only valid when
                                 // |EXTRACT_VALUE| is set.
                                 // This is used for form submission where
                                 // human readable value is captured.
  EXTRACT_OPTIONS = 1 << 2,      // Extract options from
                                 // WebFormControlElement.
};

const ExtractMask kFormExtractMasks[] = {
    EXTRACT_NONE, EXTRACT_VALUE, EXTRACT_OPTION_TEXT, EXTRACT_OPTIONS,
};

// Gets the attributes to check for a mask in |kFormExtractMasks|.
NSArray* GetFormFieldAttributeListsToCheck(NSUInteger mask) {
  if (!(mask & EXTRACT_VALUE)) {
    return @[
      @"identifier", @"name", @"form_control_type", @"autocomplete_attribute",
      @"max_length", @"should_autocomplete", @"is_checkable"
    ];
  }

  if (mask & EXTRACT_OPTIONS) {
    return @[
      @"identifier", @"name", @"form_control_type", @"autocomplete_attribute",
      @"max_length", @"should_autocomplete", @"is_checkable", @"value",
      @"option_values", @"option_contents"
    ];
  }

  return @[
    @"identifier", @"name", @"form_control_type", @"autocomplete_attribute",
    @"max_length", @"should_autocomplete", @"is_checkable", @"value"
  ];
}

// ***** Clang-formatting is disabled for the following block of testdata *****
// clang-format off

// Getters for form control element testing data. The returned data is
// an array consisting of an html fragment followed by an array
// of dictionaries containing the expected field attributes for elements in the
// given html fragment.
NSArray* GetTestFormInputElementWithLabelFromPrevious() {
  return @[
      @("* First name: "
          "<INPUT type='text' name='firstname' id='firstname' value='John'/>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* First name:'", @"label",
          @"'firstname'", @"identifier",
          @"'firstname'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'John'", @"value",
          @"'John'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromEnclosingLabelBefore() {
  return @[
      @("<LABEL>* First name: "
          "<INPUT type='text' name='firstname' id='firstname' value='John'/>"
          "</LABEL>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* First name:'", @"label",
          @"'firstname'", @"name",
          @"'firstname'", @"identifier",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'John'", @"value",
          @"'John'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromPreviousSpan() {
  return @[
      @("* Last name<span>:</span> "
          "<INPUT type='text' name='lastname' id='lastname' value='John'/>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* Last name:'", @"label",
          @"'lastname'", @"identifier",
          @"'lastname'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'John'", @"value",
          @"'John'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromPreviousParagraph() {
  return @[
      @("<p>* Email:</p> "
          "<INPUT type='email' name='email' id='email' "
          "value='john@example.com'/>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* Email:'", @"label",
          @"'email'", @"identifier",
          @"'email'", @"name",
          @"'email'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'john@example.com'", @"value",
          @"'john@example.com'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromPreviousLabel() {
  return @[
      @("<label>* Telephone: </label> "
          "<INPUT type='tel' id='telephone' value='12345678'/>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* Telephone:'", @"label",
          @"'telephone'", @"identifier",
          @"'telephone'", @"name",
          @"'tel'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'12345678'", @"value",
          @"'12345678'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromPreviousLabelOtherIgnored() {
  return @[
      @("Other Text <label>* Blog:</label> "
          "<INPUT type='url' id='blog' autocomplete='off' "
          "value='www.jogh.blog'/>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* Blog:'", @"label",
          @"'blog'", @"identifier",
          @"'blog'", @"name",
          @"'url'", @"form_control_type",
          @"'off'", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"false", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'www.jogh.blog'", @"value",
          @"'www.jogh.blog'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromPreviousTextSpanBr() {
  return @[
      @("* Expected visits<span>:</span> <br>"
          "<INPUT type='number' id='number' "
          "name='expected number of clicks'/>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* Expected visits:'", @"label",
          @"'number'", @"identifier",
          @"'expected number of clicks'", @"name",
          @"'number'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"''", @"value",
          @"''", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromPreviousTextBrAndSpan() {
  return @[
      @("Other <br> * Password<span>:</span> "
          "<INPUT type='password' autocomplete='off' name='pwd' id='pwd'/>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* Password:'", @"label",
          @"'pwd'", @"identifier",
          @"'pwd'", @"name",
          @"'password'", @"form_control_type",
          @"'off'", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"false", @"should_autocomplete",
          @"false", @"is_checkable",
          @"''", @"value",
          @"''", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromListItem() {
  return @[
      @("<LI>"
          "<LABEL><EM>*</EM> Code:</LABEL>"
          "<INPUT type='text' id='first code' value='415'/>"
          "<INPUT type='text' id='middle code' value='555'/>"
          "<INPUT type='text' id='last code' value='1212'/>"
          "</LI>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* Code:'", @"label",
          @"'first code'", @"identifier",
          @"'first code'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'415'", @"value",
          @"'415'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil],
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* Code:'", @"label",
          @"'middle code'", @"identifier",
          @"'middle code'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'555'", @"value",
          @"'555'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil],
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* Code:'", @"label",
          @"'last code'", @"identifier",
          @"'last code'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'1212'", @"value",
          @"'1212'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromTableColumnTD() {
  return @[
      @("<TABLE>"
          "<TR>"
          "  <TD>* First name:</TD>"
          "  <TD><INPUT type='text' id='tabletdname' value='John'/></TD>"
          "</TR>"
          "<TR>"
          "  <TD>Email:</TD>"
          "  <TD><INPUT type='email' id='tabletdemail'"
          "             value='john@example.com'/></TD>"
          "</TR>"
          "</TABLE>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* First name:'", @"label",
          @"'tabletdname'", @"identifier",
          @"'tabletdname'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'John'", @"value",
          @"'John'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil],
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'Email:'", @"label",
          @"'tabletdemail'", @"identifier",
          @"'tabletdemail'", @"name",
          @"'email'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'john@example.com'", @"value",
          @"'john@example.com'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromTableColumnTH() {
  return @[
      @("<TABLE>"
          "<TR>"
          "  <TH>* First name:</TH>"
          "  <TD><INPUT type='text' name='nameintableth' id='nameintableth'"
          "             value='John'/></TD>"
          "</TR>"
          "<TR>"
          "  <TD>Email:</TD>"
          "  <TD><INPUT type='email' id='emailtableth'"
          "             value='john@example.com'/></TD>"
          "</TR>"
          "</TABLE>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* First name:'", @"label",
          @"'nameintableth'", @"identifier",
          @"'nameintableth'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'John'", @"value",
          @"'John'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil],
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'Email:'", @"label",
          @"'emailtableth'",  @"identifier",
          @"'emailtableth'",  @"name",
          @"'email'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'john@example.com'", @"value",
          @"'john@example.com'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromTableNested() {
  return @[
      @("<TABLE>"
          "<TR>"
          "  <TD><FONT>* First </FONT><FONT>name:</FONT></TD>"
          "  <TD><INPUT type='text' id='nametablenested' value='John'/></TD>"
          "</TR>"
          "</TABLE>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* First name:'", @"label",
          @"'nametablenested'", @"identifier",
          @"'nametablenested'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'John'", @"value",
          @"'John'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromTableRow() {
  return @[
      @("<TABLE>"
          "<TR>"
          "  <TD>* <FONT>First </FONT><FONT>name:</FONT></TD>"
          "</TR>"
          "<TR>"
          "  <TD><INPUT type='text'  name='nametablerow'  id='nametablerow'"
          "             value='John'/></TD>"
          "</TR>"
          "</TABLE>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* First name:'", @"label",
          @"'nametablerow'", @"identifier",
          @"'nametablerow'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'John'", @"value",
          @"'John'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromDivTable() {
  return @[
      @("<DIV>* First name:<BR>"
          "<SPAN>"
          "<INPUT type='text' name='namedivtable' id='namedivtable'"
          "       value='John'>"
          "</SPAN>"
          "</DIV>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* First name:'", @"label",
          @"'namedivtable'", @"identifier",
          @"'namedivtable'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'John'", @"value",
          @"'John'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormInputElementWithLabelFromDefinitionList() {
  return @[
      @("<DL>"
          "  <DT>"
          "    <SPAN>"
          "      *"
          "    </SPAN>"
          "    <SPAN>"
          "      Favorite Sport"
          "    </SPAN>"
          "  </DT>"
          "  <DD>"
          "    <FONT>"
          "      <INPUT type='favorite sport' name='sport' id='sport'"
          "             value='Tennis'/>"
          "    </FONT>"
          "  </DD>"
          " </DL>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'* Favorite Sport'", @"label",
          @"'sport'", @"identifier",
          @"'sport'", @"name",
          @"'text'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          GetDefaultMaxLengthString(), @"max_length",
          @"true", @"should_autocomplete",
          @"false", @"is_checkable",
          @"'Tennis'", @"value",
          @"'Tennis'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestInputRadio() {
  return @[
      @("<input type='radio' name='boolean'  id='boolean1' value='true'/> True"
          "<input type='radio' name='boolean'  id='boolean2' value='false'/> "
          "False"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'True'", @"label",
          @"'boolean1'", @"identifier",
          @"'boolean'", @"name",
          @"'radio'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          @"undefined", @"max_length",
          @"true", @"should_autocomplete",
          @"true", @"is_checkable",
          @"'true'", @"value",
          @"'true'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil],
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'False'", @"label",
          @"'boolean2'", @"identifier",
          @"'boolean'", @"name",
          @"'radio'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          @"undefined", @"max_length",
          @"true", @"should_autocomplete",
          @"true", @"is_checkable",
          @"'false'", @"value",
          @"'false'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestInputCheckbox() {
  return @[
      @("<input type='checkbox' name='vehicle' id='vehicle1' value='Bike'> "
          "Bicycle"
          "<input type='checkbox' name='vehicle' id='vehicle2' value='Car'> "
          "Automobile"
          "<input type='checkbox' name='vehicle' id='vehicle3' value='Rocket'> "
          "Missile"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'Bicycle'", @"label",
          @"'vehicle1'", @"identifier",
          @"'vehicle'", @"name",
          @"'checkbox'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          @"undefined", @"max_length",
          @"true", @"should_autocomplete",
          @"true", @"is_checkable",
          @"'Bike'", @"value",
          @"'Bike'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil],
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'Automobile'", @"label",
          @"'vehicle2'", @"identifier",
          @"'vehicle'", @"name",
          @"'checkbox'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          @"undefined", @"max_length",
          @"true", @"should_autocomplete",
          @"true", @"is_checkable",
          @"'Car'", @"value",
          @"'Car'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil],
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'Missile'", @"label",
          @"'vehicle3'", @"identifier",
          @"'vehicle'", @"name",
          @"'checkbox'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          @"undefined", @"max_length",
          @"true", @"should_autocomplete",
          @"true", @"is_checkable",
          @"'Rocket'", @"value",
          @"'Rocket'", @"value_option_text",
          @"undefined", @"option_values",
          @"undefined", @"option_contents",
          nil]];
}

NSArray* GetTestFormSelectElement() {
  return @[
      @("  <label>State:"
          "    <select name='state' id='state'>"
          "      <option value='CA'>California</option>"
          "      <option value='TX'>Texas</option>"
          "    </select>"
          "   </label>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'State:'", @"label",
          @"'state'", @"identifier",
          @"'state'", @"name",
          @"'select-one'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          @"undefined", @"max_length",
          @"true", @"should_autocomplete",
          @"undefined", @"is_checkable",
          @"'CA'", @"value",
          @"'California'", @"value_option_text",
          @[@"'CA'", @"'TX'"], @"option_values",
          @[@"'California'", @"'Texas'"], @"option_contents",
          nil]];
}

NSArray* GetTestFormSelectElementWithOptgroup() {
  return @[
      @("  <label>Course:"
          "    <select name='course' id='course'>"
          "     <optgroup label='8.01 Physics I: Classical Mechanics'>"
          "      <option value='8.01.1'>Lecture 01: Powers of Ten"
          "      <option value='8.01.2'>Lecture 02: 1D Kinematics"
          "      <option value='8.01.3'>Lecture 03: Vectors"
          "     <optgroup label='8.02 Electricity and Magnestism'>"
          "      <option value='8.02.1'>Lecture 01: What holds world together?"
          "      <option value='8.02.2'>Lecture 02: Electric Field"
          "      <option value='8.02.3'>Lecture 03: Electric Flux"
          "    </select>"
          "  </label>"),
      [NSDictionary dictionaryWithObjectsAndKeys:
          @"'Course:'", @"label",
          @"'course'", @"identifier",
          @"'course'", @"name",
          @"'select-one'", @"form_control_type",
          @"undefined", @"autocomplete_attribute",
          @"undefined", @"max_length",
          @"true", @"should_autocomplete",
          @"undefined", @"is_checkable",
          @"'8.01.1'", @"value",
          @"'Lecture 01: Powers of Ten'", @"value_option_text",
          @[@"'8.01.1'",
              @"'8.01.2'",
              @"'8.01.3'",
              @"'8.02.1'",
              @"'8.02.2'",
              @"'8.02.3'"], @"option_values",
          @[@"'Lecture 01: Powers of Ten'",
              @"'Lecture 02: 1D Kinematics'",
              @"'Lecture 03: Vectors'",
              @"'Lecture 01: What holds world together?'",
              @"'Lecture 02: Electric Field'",
              @"'Lecture 03: Electric Flux'"], @"option_contents",
          nil]];
}

// clang-format on

// Generates JavaScripts to check a JavaScripts object |results| with the
// expected values given in |expected|, which is a dictionary with string
// values for all the keys other than @"option_vaues" and @"option_contents";
// the values of @"option_vaues" and @"option_contents" are arrays of
// strings or undefined. Only attributes in |attributes_to_check| are checked.
// A different expected value is chosen in |expected| for different
// |extract_mask|.
// |index| is the index of the control element in the form. If it is >0, it will
// be used to generate a name for nameless elements.
NSString* GenerateElementItemVerifyingJavaScripts(NSString* results,
                                                  NSUInteger extract_mask,
                                                  NSDictionary* expected,
                                                  NSArray* attributes_to_check,
                                                  int index) {
  NSMutableArray* verifying_javascripts = [NSMutableArray array];

  for (NSString* attribute in attributes_to_check) {
    if ([attribute isEqualToString:@"option_values"] ||
        [attribute isEqualToString:@"option_contents"]) {
      id expected_value = [expected objectForKey:attribute];
      if ([expected_value isKindOfClass:[NSString class]]) {
        [verifying_javascripts
            addObject:[NSString
                          stringWithFormat:@"%@['%@']===%@", results, attribute,
                                           [expected objectForKey:attribute]]];
      } else {
        for (NSUInteger i = 0; i < [(NSArray*)expected_value count]; ++i) {
          [verifying_javascripts
              addObject:[NSString
                            stringWithFormat:@"%@['%@'][%" PRIuNS "] === %@",
                                             results, attribute, i,
                                             [expected_value objectAtIndex:i]]];
        }
      }
    } else {
      NSString* expected_value = [expected objectForKey:attribute];
      if ([attribute isEqualToString:@"name"] &&
          [expected_value isEqualToString:@"''"] && index >= 0) {
        expected_value =
            [NSString stringWithFormat:@"'gChrome~field~%d'", index];
      }
      // Option text is used as value for extract_mask 1 << 1
      if ((extract_mask & 1 << 1) && [attribute isEqualToString:@"value"])
        expected_value = [expected objectForKey:@"value_option_text"];
      [verifying_javascripts
          addObject:[NSString stringWithFormat:@"%@['%@']===%@", results,
                                               attribute, expected_value]];
    }
  }

  return [verifying_javascripts componentsJoinedByString:@"&&"];
}

// Generates JavaScripts to check a JavaScripts array |results| with the
// expected values given in |expected|, which is an array of dictionaries; each
// dictionary is the expected values of the corresponding item in |results|.
// Only attributes in |attributes_to_check| are checked. A different expected
// value is chosen in |expected| for different |extract_mask|.
NSString* GenerateTestItemVerifyingJavaScripts(NSString* results,
                                               NSUInteger extract_mask,
                                               NSArray* expected,
                                               NSArray* attributed_to_check) {
  NSMutableArray* verifying_javascripts = [NSMutableArray array];

  NSUInteger controlCount = 0;
  for (NSUInteger indexOfTestData = 0; indexOfTestData < [expected count];
       ++indexOfTestData) {
    NSArray* expectedData = [expected objectAtIndex:indexOfTestData];
    for (NSUInteger i = 1; i < [expectedData count]; ++i, ++controlCount) {
      NSDictionary* expected = [expectedData objectAtIndex:i];
      NSString* itemVerifyingJavaScripts =
          GenerateElementItemVerifyingJavaScripts(
              [NSString stringWithFormat:@"%@['fields'][%" PRIuNS "]", results,
                                         controlCount],
              extract_mask, expected, attributed_to_check, controlCount);
      [verifying_javascripts addObject:itemVerifyingJavaScripts];
    }
  }
  return [verifying_javascripts componentsJoinedByString:@"&&"];
}

// Test fixture to test autofill controller.
class AutofillControllerJsTest : public web::WebJsTest<ChromeWebTest> {
 public:
  AutofillControllerJsTest()
      : web::WebJsTest<ChromeWebTest>(std::make_unique<ChromeWebClient>()) {}

 protected:
  // Helper method that EXPECTs |javascript| evaluation on page
  // |kHTMLForTestingElements| with expectation given by
  // |elements_with_true_expected|.
  void TestExecutingBooleanJavaScriptOnElement(
      NSString* javascript,
      const ElementByName elements_with_true_expected[],
      size_t size_elements_with_true_expected);

  // Helper method that EXPECTs
  // |__gCrWeb.fill.webFormControlElementToFormField|. This method applies
  // |__gCrWeb.fill.webFormControlElementToFormField| on each element in
  // |test_data| with all possible extract masks and verify the results.
  void TestWebFormControlElementToFormField(NSArray* test_data,
                                            NSString* tag_name);

  // Helper method for testing |javascripts_statement| that evalutate
  // |attribute_name| of the elements in |test_data| which has tag name
  // |tag_name|. EXPECTs JavaScript evaluation on
  // "window.document.getElementsByTagName()"
  void TestInputElementDataEvaluation(NSString* javascripts_statement,
                                      NSString* attribute_name,
                                      NSArray* test_data,
                                      NSString* tag_name);

  // Helper method that EXPECTs |__gCrWeb.fill.webFormElementToFormData| on
  // a form element obtained by |get_form_element_javascripts|. The results
  // are verified with |verifying_java_scripts|.
  void TestWebFormElementToFormDataForOneForm(
      NSString* get_form_element_javascripts,
      NSUInteger extract_mask,
      NSString* expected_result,
      NSString* verifying_javascripts);

  // EXPECTs |__gCrWeb.fill.webFormElementToFormData| on all the test data.
  void TestWebFormElementToFormData(NSArray* test_items);

  // EXPECTs |__gCrWeb.autofill.extractNewForms| on |html|.
  void TestExtractNewForms(NSString* html,
                           BOOL is_origin_window_location,
                           NSArray* expected_items);
};

void AutofillControllerJsTest::TestExecutingBooleanJavaScriptOnElement(
    NSString* javascript,
    const ElementByName elements_with_true_expected[],
    size_t size_elements_with_true_expected) {
  // Elements in |kHTMLForTestingElements|.
  const ElementByName elementsByName[] = {
      {"hl", 0, -1},
      {"firstname", 0, -1},
      {"lastname", 0, -1},
      {"email", 0, -1},
      {"phone", 0, -1},
      {"blog", 0, -1},
      {"expected number of clicks", 0, -1},
      {"pwd", 0, -1},
      {"vehicle", 0, -1},
      {"vehicle", 1, -1},
      {"vehicle", 2, -1},
      {"boolean", 0, -1},
      {"boolean", 1, -1},
      {"boolean", 2, -1},
      {"state", 0, -1},
      {"state", 0, 0},
      {"state", 0, 1},
      {"course", 0, -1},
      {"course", 0, 0},
      {"course", 0, 1},
      {"course", 0, 2},
      {"course", 0, 3},
      {"course", 0, 4},
      {"course", 0, 5},
      {"cars", 0, -1},
      {"cars", 0, 0},
      {"cars", 0, 1},
      {"cars", 0, 2},
      {"cars", 0, 3},
      {"submit", 0, -1},
  };

  LoadHtml(kHTMLForTestingElements);
  ExecuteBooleanJavaScriptOnElementsAndCheck(
      javascript,
      GetElementsByNameJavaScripts(elementsByName, base::size(elementsByName)),
      GetElementsByNameJavaScripts(elements_with_true_expected,
                                   size_elements_with_true_expected));
}

TEST_F(AutofillControllerJsTest, HasTagName) {
  const ElementByName elements_expecting_true[] = {
      {"hl", 0, -1},
      {"firstname", 0, -1},
      {"lastname", 0, -1},
      {"email", 0, -1},
      {"phone", 0, -1},
      {"blog", 0, -1},
      {"expected number of clicks", 0, -1},
      {"pwd", 0, -1},
      {"vehicle", 0, -1},
      {"vehicle", 1, -1},
      {"vehicle", 2, -1},
      {"boolean", 0, -1},
      {"boolean", 1, -1},
      {"boolean", 2, -1},
      {"submit", 0, -1},
  };

  TestExecutingBooleanJavaScriptOnElement(
      @"__gCrWeb.fill.hasTagName(%@, 'input')", elements_expecting_true,
      base::size(elements_expecting_true));
}

TEST_F(AutofillControllerJsTest, CombineAndCollapseWhitespace) {
  LoadHtml(@"<html><body></body></html>");

  EXPECT_NSEQ(@"foobar", ExecuteJavaScriptWithFormat(
                             @"__gCrWeb.fill.combineAndCollapseWhitespace('"
                             @"foo', 'bar', false)"));
  EXPECT_NSEQ(@"foo bar", ExecuteJavaScriptWithFormat(
                              @"__gCrWeb.fill.combineAndCollapseWhitespace("
                              @"'foo', 'bar', true)"));
  EXPECT_NSEQ(@"foo bar", ExecuteJavaScriptWithFormat(
                              @"__gCrWeb.fill.combineAndCollapseWhitespace("
                               "'foo ', 'bar', false)"));
  EXPECT_NSEQ(@"foo bar", ExecuteJavaScriptWithFormat(
                              @"__gCrWeb.fill.combineAndCollapseWhitespace("
                               "'foo', ' bar', false)"));
  EXPECT_NSEQ(@"foo bar", ExecuteJavaScriptWithFormat(
                              @"__gCrWeb.fill.combineAndCollapseWhitespace("
                               "'foo', ' bar', true)"));
  EXPECT_NSEQ(@"foo bar", ExecuteJavaScriptWithFormat(
                              @"__gCrWeb.fill.combineAndCollapseWhitespace("
                               "'foo  ', '  bar', false)"));
  EXPECT_NSEQ(@"foobar ", ExecuteJavaScriptWithFormat(
                              @"__gCrWeb.fill.combineAndCollapseWhitespace("
                               "'foo', 'bar ', false)"));
  EXPECT_NSEQ(@" foo bar", ExecuteJavaScriptWithFormat(
                               @"__gCrWeb.fill.combineAndCollapseWhitespace("
                                "' foo', 'bar', true)"));
}

void AutofillControllerJsTest::TestInputElementDataEvaluation(
    NSString* javascripts_statement,
    NSString* attribute_name,
    NSArray* test_data,
    NSString* tag_name) {
  NSString* html_fragment = [test_data objectAtIndex:0U];
  LoadHtml(html_fragment);

  for (NSUInteger i = 1; i < [test_data count]; ++i) {
    NSString* get_element_javascripts = [NSString
        stringWithFormat:@"window.document.getElementsByTagName('%@')[%" PRIuNS
                          "]",
                         tag_name, i - 1];
    id actual = ExecuteJavaScriptWithFormat(
        @"%@(%@) === %@", javascripts_statement, get_element_javascripts,
        [[test_data objectAtIndex:i] objectForKey:attribute_name]);
    EXPECT_NSEQ(@YES, actual);
  }
}

TEST_F(AutofillControllerJsTest, InferLabelFromPrevious) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromPrevious", @"label",
      GetTestFormInputElementWithLabelFromPrevious(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromPreviousSpan) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromPrevious", @"label",
      GetTestFormInputElementWithLabelFromPreviousSpan(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromPreviousParagraph) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromPrevious", @"label",
      GetTestFormInputElementWithLabelFromPreviousParagraph(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromPreviousLabel) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromPrevious", @"label",
      GetTestFormInputElementWithLabelFromPreviousLabel(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromPreviousLabelOtherIgnored) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromPrevious", @"label",
      GetTestFormInputElementWithLabelFromPreviousLabelOtherIgnored(),
      @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromEnclosingLabelBefore) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromEnclosingLabel", @"label",
      GetTestFormInputElementWithLabelFromEnclosingLabelBefore(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromPreviousTextBrAndSpan) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromPrevious", @"label",
      GetTestFormInputElementWithLabelFromPreviousTextBrAndSpan(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromListItem) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromListItem", @"label",
      GetTestFormInputElementWithLabelFromListItem(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromTableColumnTD) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromTableColumn", @"label",
      GetTestFormInputElementWithLabelFromTableColumnTD(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromTableColumnTH) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromTableColumn", @"label",
      GetTestFormInputElementWithLabelFromTableColumnTH(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromTableColumnNested) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromTableColumn", @"label",
      GetTestFormInputElementWithLabelFromTableNested(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromTableRow) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromTableRow", @"label",
      GetTestFormInputElementWithLabelFromTableRow(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromDivTable) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromDivTable", @"label",
      GetTestFormInputElementWithLabelFromDivTable(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelFromDefinitionList) {
  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelFromDefinitionList", @"label",
      GetTestFormInputElementWithLabelFromDefinitionList(), @"input");
}

TEST_F(AutofillControllerJsTest, InferLabelForElement) {
  NSArray* testingElements = @[
    GetTestFormInputElementWithLabelFromPrevious(),
    GetTestFormInputElementWithLabelFromPreviousSpan(),
    GetTestFormInputElementWithLabelFromPreviousParagraph(),
    GetTestFormInputElementWithLabelFromPreviousLabel(),
    GetTestFormInputElementWithLabelFromPreviousLabelOtherIgnored(),
    GetTestFormInputElementWithLabelFromEnclosingLabelBefore(),
    GetTestFormInputElementWithLabelFromPreviousTextSpanBr(),
    GetTestFormInputElementWithLabelFromPreviousTextBrAndSpan(),
    GetTestFormInputElementWithLabelFromListItem(),
    GetTestFormInputElementWithLabelFromTableColumnTD(),
    GetTestFormInputElementWithLabelFromTableColumnTH(),
    GetTestFormInputElementWithLabelFromTableNested(),
    GetTestFormInputElementWithLabelFromTableRow(),
    GetTestFormInputElementWithLabelFromDivTable(),
    GetTestFormInputElementWithLabelFromDefinitionList(), GetTestInputRadio(),
    GetTestInputCheckbox()
  ];
  for (NSArray* testingElement in testingElements) {
    TestInputElementDataEvaluation(@"__gCrWeb.fill.inferLabelForElement",
                                   @"label", testingElement, @"input");
  }

  TestInputElementDataEvaluation(@"__gCrWeb.fill.inferLabelForElement",
                                 @"label", GetTestFormSelectElement(),
                                 @"select");

  TestInputElementDataEvaluation(
      @"__gCrWeb.fill.inferLabelForElement", @"label",
      GetTestFormSelectElementWithOptgroup(), @"select");
}

TEST_F(AutofillControllerJsTest, IsAutofillableElement) {
  const ElementByName elements_expecting_true[] = {
      {"firstname", 0, -1}, {"lastname", 0, -1},
      {"email", 0, -1},     {"phone", 0, -1},
      {"blog", 0, -1},      {"expected number of clicks", 0, -1},
      {"pwd", 0, -1},       {"vehicle", 0, -1},
      {"vehicle", 1, -1},   {"vehicle", 2, -1},
      {"boolean", 0, -1},   {"boolean", 1, -1},
      {"boolean", 2, -1},   {"state", 0, -1},
      {"course", 0, -1},
  };

  TestExecutingBooleanJavaScriptOnElement(
      @"__gCrWeb.fill.isAutofillableElement(%@)", elements_expecting_true,
      base::size(elements_expecting_true));
}

TEST_F(AutofillControllerJsTest, GetOptionStringsFromElement) {
  ElementByName testing_elements[] = {
      {"state", 0, -1}, {"course", 0, -1}, {"cars", 0, -1}};

  LoadHtml(kHTMLForTestingElements);
  ExecuteJavaScriptOnElementsAndCheck(
      @"var field = {};"
       "__gCrWeb.fill.getOptionStringsFromElement(%@, field);"
       "__gCrWeb.stringify(field);",
      GetElementsByNameJavaScripts(testing_elements,
                                   base::size(testing_elements)),
      @[
        @("{\"option_values\":[\"CA\",\"MA\"],"
          "\"option_contents\":[\"CA\",\"MA\"]}"),
        @("{\"option_values\":["
          "\"8.01.1\",\"8.01.2\",\"8.01.3\","
          "\"8.02.1\",\"8.02.2\",\"8.02.3\"],"
          "\"option_contents\":["
          "\"Lecture 01: Powers of Ten\","
          "\"Lecture 02: 1D Kinematics\","
          "\"Lecture 03: Vectors\","
          "\"Lecture 01: What holds our world together?\","
          "\"Lecture 02: Electric Field\","
          "\"Lecture 03: Electric Flux\""
          "]}"),
        @("{\"option_values\":[\"volvo\",\"saab\",\"opel\",\"audi\"],"
          "\"option_contents\":[\"Volvo\",\"Saab\",\"Opel\",\"Audi\"]}")
      ]);
}

TEST_F(AutofillControllerJsTest, FillFormField) {
  LoadHtml(kHTMLForTestingElements);

  // Test text and select elements of which the value should be changed.
  const ElementByName elements[] = {
      {"firstname", 0, -1}, {"state", 0, -1},
  };
  NSArray* values = @[
    @"new name",
    @"MA",
  ];
  for (size_t i = 0; i < base::size(elements); ++i) {
    NSString* get_element_javascript = GetElementByNameJavaScript(elements[i]);
    NSString* new_value = [values objectAtIndex:i];
    EXPECT_NSEQ(new_value,
                ExecuteJavaScriptWithFormat(
                    @"var element=%@;var data={'value':'%@'};"
                    @"__gCrWeb.autofill.fillFormField(data, element);"
                    @"element.value",
                    get_element_javascript, new_value));
  }

  // Test clickable elements, of which 'checked' should be updated.
  ElementByName checkable_elements[] = {
      {"vehicle", 0, -1}, {"vehicle", 1, -1}, {"vehicle", 2, -1},
      {"boolean", 0, -1}, {"boolean", 1, -1}, {"boolean", 2, -1},
  };
  const bool final_is_checked_values[] = {
      true, false, true, false, true, true,
  };
  for (size_t i = 0; i < base::size(checkable_elements); ++i) {
    NSString* get_element_javascript =
        GetElementByNameJavaScript(checkable_elements[i]);
    bool is_checked = final_is_checked_values[i];

    EXPECT_NSEQ(
        @(is_checked),
        ExecuteJavaScriptWithFormat(
            @"var element=%@; var value=element.value; "
            @"var data={'value':value,'is_checked':%@};"
            @"__gCrWeb.autofill.fillFormField(data, element); element.checked",
            get_element_javascript, is_checked ? @"true" : @"false"));
  }

  // Test elements of which the value should not be changed.
  ElementByName unchanged_elements[] = {
      {"hl", 0, -1},    // hidden element
      {"state", 0, 0},  // option element
      {"state", 0, 1},  // option element
  };
  for (size_t i = 0; i < base::size(unchanged_elements); ++i) {
    NSString* get_element_javascript =
        GetElementByNameJavaScript(unchanged_elements[i]);
    NSString* actual = ExecuteJavaScriptWithFormat(
        @"var element=%@;"
        @"var oldValue=element.value; var data={'value':'new'};"
        @"__gCrWeb.autofill.fillFormField(data, element);"
        @"element.value === oldValue",
        get_element_javascript);
    EXPECT_NSEQ(@YES, actual);
  }
}

TEST_F(AutofillControllerJsTest, IsTextInput) {
  const ElementByName elements_expecting_true[] = {
      {"firstname", 0, -1}, {"lastname", 0, -1},
      {"email", 0, -1},     {"phone", 0, -1},
      {"blog", 0, -1},      {"expected number of clicks", 0, -1},
      {"pwd", 0, -1},
  };

  TestExecutingBooleanJavaScriptOnElement(@"__gCrWeb.fill.isTextInput(%@)",
                                          elements_expecting_true,
                                          base::size(elements_expecting_true));
}

TEST_F(AutofillControllerJsTest, IsSelectElement) {
  const ElementByName elements_expecting_true[] = {
      {"state", 0, -1}, {"course", 0, -1},
  };

  TestExecutingBooleanJavaScriptOnElement(@"__gCrWeb.fill.isSelectElement(%@)",
                                          elements_expecting_true,
                                          base::size(elements_expecting_true));
}

TEST_F(AutofillControllerJsTest, IsCheckableElement) {
  const ElementByName elements_expecting_true[] = {
      {"vehicle", 0, -1}, {"vehicle", 1, -1}, {"vehicle", 2, -1},
      {"boolean", 0, -1}, {"boolean", 1, -1}, {"boolean", 2, -1},
  };

  TestExecutingBooleanJavaScriptOnElement(
      @"__gCrWeb.fill.isCheckableElement(%@)", elements_expecting_true,
      base::size(elements_expecting_true));
}

TEST_F(AutofillControllerJsTest, IsAutofillableInputElement) {
  const ElementByName elements_expecting_true[] = {
      {"firstname", 0, -1}, {"lastname", 0, -1},
      {"email", 0, -1},     {"phone", 0, -1},
      {"blog", 0, -1},      {"expected number of clicks", 0, -1},
      {"pwd", 0, -1},       {"vehicle", 0, -1},
      {"vehicle", 1, -1},   {"vehicle", 2, -1},
      {"boolean", 0, -1},   {"boolean", 1, -1},
      {"boolean", 2, -1},
  };

  TestExecutingBooleanJavaScriptOnElement(
      @"__gCrWeb.fill.isAutofillableInputElement(%@)", elements_expecting_true,
      base::size(elements_expecting_true));
}

TEST_F(AutofillControllerJsTest, ExtractAutofillableElements) {
  LoadHtml(kHTMLForTestingElements);
  ElementByName expected_elements[] = {
      {"firstname", 0, -1}, {"lastname", 0, -1},
      {"email", 0, -1},     {"phone", 0, -1},
      {"blog", 0, -1},      {"expected number of clicks", 0, -1},
      {"pwd", 0, -1},       {"vehicle", 0, -1},
      {"vehicle", 1, -1},   {"vehicle", 2, -1},
      {"boolean", 0, -1},   {"boolean", 1, -1},
      {"boolean", 2, -1},   {"state", 0, -1},
  };
  NSArray* expected = GetElementsByNameJavaScripts(
      expected_elements, base::size(expected_elements));

  NSString* parameter = @"window.document.getElementsByTagName('form')[0]";
  for (NSUInteger index = 0; index < [expected count]; index++) {
    EXPECT_NSEQ(@YES,
                ExecuteJavaScriptWithFormat(
                    @"var controlElements="
                     "__gCrWeb.autofill.extractAutofillableElementsInForm(%@);"
                     "controlElements[%" PRIuNS "] === %@",
                    parameter, index, expected[index]));
  }
}

void AutofillControllerJsTest::TestWebFormControlElementToFormField(
    NSArray* test_data,
    NSString* tag_name) {
  LoadHtml([test_data firstObject]);

  for (NSUInteger i = 0; i < base::size(kFormExtractMasks); ++i) {
    ExtractMask extract_mask = kFormExtractMasks[i];
    NSArray* attributes_to_check =
        GetFormFieldAttributeListsToCheck(extract_mask);

    for (NSUInteger i = 1; i < [test_data count]; ++i) {
      NSString* get_element_to_test =
          [NSString stringWithFormat:@"var element = "
                                      "window.document.getElementsByTagName('%"
                                      "@')[%" PRIuNS "]",
                                     tag_name, i - 1];
      NSDictionary* expected = [test_data objectAtIndex:i];
      // Generates JavaScripts to verify the results. Parameter |results| is
      // @"field" as in the evaluation JavaScripts the results are returned in
      // |field|.
      NSString* verifying_javascripts = GenerateElementItemVerifyingJavaScripts(
          @"field", extract_mask, expected, attributes_to_check, -1);
      EXPECT_NSEQ(@YES,
                  ExecuteJavaScriptWithFormat(
                      @"%@; var field = {};"
                       "__gCrWeb.fill.webFormControlElementToFormField("
                       "    element, %u, field);"
                       "%@",
                      get_element_to_test, extract_mask, verifying_javascripts))
          << base::SysNSStringToUTF8([NSString
                 stringWithFormat:
                     @"webFormControlElementToFormField actual results are: "
                     @"%@, \n"
                      "expected to be verified by %@",
                     ExecuteJavaScriptWithFormat(
                         @"%@; var field = {};"
                          "__gCrWeb.fill.webFormControlElementToFormField("
                          "    element, %u, field);__gCrWeb.stringify(field);",
                         get_element_to_test, extract_mask),
                     verifying_javascripts]);
    }
  }
}

TEST_F(AutofillControllerJsTest, WebFormControlElementToFormField) {
  NSArray* test_input_elements = @[
    GetTestFormInputElementWithLabelFromPrevious(),
    GetTestFormInputElementWithLabelFromPreviousSpan(),
    GetTestFormInputElementWithLabelFromPreviousParagraph(),
    GetTestFormInputElementWithLabelFromPreviousLabel(),
    GetTestFormInputElementWithLabelFromPreviousLabelOtherIgnored(),
    GetTestFormInputElementWithLabelFromEnclosingLabelBefore(),
    GetTestFormInputElementWithLabelFromPreviousTextBrAndSpan(),
    GetTestFormInputElementWithLabelFromPreviousTextSpanBr(),
    GetTestFormInputElementWithLabelFromListItem(),
    GetTestFormInputElementWithLabelFromTableColumnTD(),
    GetTestFormInputElementWithLabelFromTableColumnTH(),
    GetTestFormInputElementWithLabelFromTableNested(),
    GetTestFormInputElementWithLabelFromTableRow(),
    GetTestFormInputElementWithLabelFromDivTable(),
    GetTestFormInputElementWithLabelFromDefinitionList(), GetTestInputRadio(),
    GetTestInputCheckbox()
  ];
  for (NSArray* test_element in test_input_elements) {
    TestWebFormControlElementToFormField(test_element, @"input");
  }

  TestWebFormControlElementToFormField(GetTestFormSelectElement(), @"select");
  TestWebFormControlElementToFormField(GetTestFormSelectElementWithOptgroup(),
                                       @"select");
}

void AutofillControllerJsTest::TestWebFormElementToFormDataForOneForm(
    NSString* get_form_element_javascripts,
    NSUInteger extract_mask,
    NSString* expected_result,
    NSString* verifying_javascripts) {
  NSString* actual = ExecuteJavaScriptWithFormat(
      @"var form={}; var field={};"
       "(__gCrWeb.fill.webFormElementToFormData("
       "window, %@, null, %" PRIuNS ", form, field) === %@) && %@",
      get_form_element_javascripts, extract_mask, expected_result,
      verifying_javascripts);

  EXPECT_NSEQ(@YES, actual) << base::SysNSStringToUTF8([NSString
      stringWithFormat:@"Actual:\n%@; expected to be verifyied by\n%@",
                       ExecuteJavaScriptWithFormat(
                           @"var form={};"
                            "__gCrWeb.fill."
                            "webFormElementToFormData(window, %@, null,"
                            "%" PRIuNS ", form, null);"
                            "__gCrWeb.stringify(form);",
                           get_form_element_javascripts, extract_mask),
                       verifying_javascripts]);
}

void AutofillControllerJsTest::TestWebFormElementToFormData(
    NSArray* test_items) {
  NSString* form_html_fragment =
      @"<form name='TestForm' action='http://cnn.com' method='post'>";
  for (NSUInteger i = 0; i < [test_items count]; ++i) {
    form_html_fragment = [form_html_fragment
        stringByAppendingString:[[test_items objectAtIndex:i]
                                    objectAtIndex:0U]];
  }
  form_html_fragment = [form_html_fragment stringByAppendingString:@"</form>"];
  LoadHtml(form_html_fragment);

  NSString* parameter = @"document.getElementsByTagName('form')[0]";
  for (NSUInteger extract_index = 0;
       extract_index < base::size(kFormExtractMasks); ++extract_index) {
    NSString* expected_result = @"true";
    // We don't verify 'action' here as action is generated as a complete url
    // and here data url is used.
    NSMutableArray* verifying_javascripts = [NSMutableArray
        arrayWithObjects:@"form['name'] === 'TestForm'",
                         @"form['origin'] === window.location.href", nil];
    ExtractMask extract_mask = kFormExtractMasks[extract_index];
    [verifying_javascripts
        addObject:GenerateTestItemVerifyingJavaScripts(
                      @"form", extract_mask, test_items,
                      GetFormFieldAttributeListsToCheck(extract_mask))];
    TestWebFormElementToFormDataForOneForm(
        parameter, extract_mask, expected_result,
        [verifying_javascripts componentsJoinedByString:@"&&"]);
  }
}

TEST_F(AutofillControllerJsTest, WebFormElementToFormData) {
  NSArray* test_elements = @[
    GetTestFormInputElementWithLabelFromPrevious(),
    GetTestFormInputElementWithLabelFromPreviousSpan(),
    GetTestFormInputElementWithLabelFromPreviousParagraph(),
    GetTestFormInputElementWithLabelFromPreviousLabel(),
    GetTestFormInputElementWithLabelFromPreviousLabelOtherIgnored(),
    GetTestFormInputElementWithLabelFromEnclosingLabelBefore(),
    GetTestFormInputElementWithLabelFromPreviousTextSpanBr(),
    GetTestFormInputElementWithLabelFromPreviousTextBrAndSpan(),
    GetTestFormInputElementWithLabelFromListItem(),
    GetTestFormInputElementWithLabelFromTableColumnTD(),
    GetTestFormInputElementWithLabelFromTableColumnTH(),
    GetTestFormInputElementWithLabelFromTableNested(),
    GetTestFormInputElementWithLabelFromTableRow(),
    GetTestFormInputElementWithLabelFromDivTable(),
    GetTestFormInputElementWithLabelFromDefinitionList(), GetTestInputRadio(),
    GetTestInputCheckbox(), GetTestFormSelectElement(),
    GetTestFormSelectElementWithOptgroup()
  ];
  // Test a form that has a signle item in the array.
  for (NSArray* testElement in test_elements) {
    TestWebFormElementToFormData(@[ testElement ]);
  }

  // Also test a form that has all the above items.
  TestWebFormElementToFormData(test_elements);
}

TEST_F(AutofillControllerJsTest, WebFormElementToFormDataTooManyFields) {
  NSString* html_fragment = @"<FORM name='Test' action='http://c.com'>";
  // In autofill_controller.js, the maximum number of parsable element is 200
  // (__gCrWeb.fill.MAX_PARSEABLE_FIELDS = 200). Here an HTML page with 201
  // elements is generated for testing.
  for (NSUInteger index = 0; index < 201; ++index) {
    html_fragment =
        [html_fragment stringByAppendingFormat:@"<INPUT type='text'/>"];
  }
  html_fragment = [html_fragment stringByAppendingFormat:@"</FORM>"];

  LoadHtml(html_fragment);
  TestWebFormElementToFormDataForOneForm(
      @"document.getElementsByTagName('form')[0]", 1, @"false", @"true");
}

TEST_F(AutofillControllerJsTest, WebFormElementToFormEmpty) {
  NSString* html_fragment = @"<FORM name='Test' action='http://c.com'>";
  html_fragment = [html_fragment stringByAppendingFormat:@"</FORM>"];

  LoadHtml(html_fragment);
  TestWebFormElementToFormDataForOneForm(
      @"document.getElementsByTagName('form')[0]", 1, @"false", @"true");
}

void AutofillControllerJsTest::TestExtractNewForms(
    NSString* html,
    BOOL is_origin_window_location,
    NSArray* expected_items) {
  LoadHtml(html);
  // Generates verifying javascripts.
  NSMutableArray* verifying_javascripts = [NSMutableArray array];
  for (NSUInteger i = 0U; i < [expected_items count]; ++i) {
    // All forms created in this test suite are named "TestForm".
    // If a page contains more than one of these forms, ExtractForms will rename
    // all forms but the fist one.
    NSString* formName =
        (i == 0) ? @"TestForm"
                 : [NSString stringWithFormat:@"gChrome~form~%" PRIuNS, i];
    [verifying_javascripts
        addObject:[NSString stringWithFormat:@"forms[%" PRIuNS
                                              "]['name'] === '%@'",
                                             i, formName]];
    if (is_origin_window_location) {
      [verifying_javascripts
          addObject:[NSString stringWithFormat:
                                  @"forms[%" PRIuNS
                                   "]['origin'] === window.location.href",
                                  i]];
    }
    // This is the extract mask used by
    // __gCrWeb.autofill.extractNewForms.
    NSUInteger extract_mask = EXTRACT_VALUE | EXTRACT_OPTIONS;
    [verifying_javascripts
        addObject:GenerateTestItemVerifyingJavaScripts(
                      [NSString stringWithFormat:@"forms[%" PRIuNS "]", i],
                      extract_mask, [expected_items objectAtIndex:i],
                      // The relevant attributes for the extract mask
                      GetFormFieldAttributeListsToCheck(extract_mask))];
  }

  NSString* actual = ExecuteJavaScriptWithFormat(
      @"var forms = __gCrWeb.autofill.extractNewForms(%" PRIuS ", true); %@",
      autofill::MinRequiredFieldsForHeuristics(),
      [verifying_javascripts componentsJoinedByString:@"&&"]);

  EXPECT_NSEQ(@YES, actual) << base::SysNSStringToUTF8([NSString
      stringWithFormat:@"actually forms = %@, "
                        "but it is expected to be verified by %@",
                       ExecuteJavaScriptWithFormat(
                           @"var forms = __gCrWeb.autofill.extractNewForms("
                            "%" PRIuS ", true); __gCrWeb.stringify(forms)",
                           autofill::MinRequiredFieldsForHeuristics()),
                       verifying_javascripts]);
}

TEST_F(AutofillControllerJsTest, ExtractFormsAndFormElements) {
  NSArray* testFirstFormItems = @[
    GetTestFormInputElementWithLabelFromPrevious(),
    GetTestFormInputElementWithLabelFromPreviousSpan(),
    GetTestFormInputElementWithLabelFromPreviousParagraph(),
    GetTestFormInputElementWithLabelFromPreviousLabel(),
    GetTestFormInputElementWithLabelFromPreviousLabelOtherIgnored(),
    GetTestFormInputElementWithLabelFromEnclosingLabelBefore(),
    GetTestFormInputElementWithLabelFromPreviousTextSpanBr(),
    GetTestFormInputElementWithLabelFromPreviousTextBrAndSpan(),
    GetTestFormInputElementWithLabelFromListItem(),
    GetTestFormInputElementWithLabelFromTableColumnTD(),
    GetTestFormInputElementWithLabelFromTableColumnTH(),
    GetTestFormInputElementWithLabelFromTableNested(),
    GetTestFormInputElementWithLabelFromTableRow(),
    GetTestFormInputElementWithLabelFromDivTable(),
    GetTestFormInputElementWithLabelFromDefinitionList(), GetTestInputRadio(),
    GetTestInputCheckbox()
  ];
  NSArray* testSecondFormItems = @[
    GetTestFormInputElementWithLabelFromDivTable(), GetTestFormSelectElement(),
    GetTestFormSelectElementWithOptgroup()
  ];
  NSArray* test_forms = @[ testFirstFormItems, testSecondFormItems ];

  NSString* html = @"<html><body>";
  for (NSArray* testFormItems in test_forms) {
    html = [html stringByAppendingString:
                     @"<form name='TestForm' action='http://c.com'>"];
    for (NSArray* testItem in testFormItems) {
      html = [html stringByAppendingString:[testItem objectAtIndex:0U]];
    }
    html = [html stringByAppendingFormat:@"</form>"];
  }
  html = [html stringByAppendingFormat:@"</body></html>"];
  TestExtractNewForms(html, true, test_forms);
}

TEST_F(AutofillControllerJsTest,
       ExtractFormsAndFormElementsWithFormAssociatedElementsOutOfForm) {
  NSString* html =
      @"<html><body>"
       "<form id='testform'></form>"
       "1 <input type='text' name='name1' id='name1' form='testform'></input>"
       "2 <input type='text' name='name2' id='name2' form='testform'></input>"
       "3 <input type='text' name='name3' id='name3' form='testform'></input>"
       "4 <input type='text' name='name4' id='name4' form='testform'></input>"
       "</body></html>";
  LoadHtml(html);

  NSString* verifying_javascript = @"forms[0]['fields'][0]['name']==='name1' &&"
                                   @"forms[0]['fields'][0]['label']==='1' &&"
                                   @"forms[0]['fields'][1]['name']==='name2' &&"
                                   @"forms[0]['fields'][1]['label']==='2' &&"
                                   @"forms[0]['fields'][2]['name']==='name3' &&"
                                   @"forms[0]['fields'][2]['label']==='3' &&"
                                   @"forms[0]['fields'][3]['name']==='name4' &&"
                                   @"forms[0]['fields'][3]['label']==='4'";
  EXPECT_NSEQ(@YES, ExecuteJavaScriptWithFormat(
                        @"var forms = "
                         "__gCrWeb.autofill.extractNewForms(1, true); %@",
                        verifying_javascript));
}

TEST_F(AutofillControllerJsTest, ExtractForms) {
  NSString* html = @"<html><body>";
  html = [html
      stringByAppendingString:@"<form name='TestForm' action='http://c.com'>"];
  html = [html
      stringByAppendingString:[GetTestFormInputElementWithLabelFromPrevious()
                                  objectAtIndex:0U]];
  html =
      [html stringByAppendingString:[GetTestInputCheckbox() objectAtIndex:0U]];
  html = [html stringByAppendingString:
                   [GetTestFormInputElementWithLabelFromTableColumnTH()
                       objectAtIndex:0U]];
  html = [html stringByAppendingString:
                   [GetTestFormInputElementWithLabelFromPreviousTextBrAndSpan()
                       objectAtIndex:0U]];
  html = [html
      stringByAppendingString:[GetTestFormSelectElement() objectAtIndex:0U]];
  html = [html stringByAppendingFormat:@"</form>"];
  html = [html stringByAppendingFormat:@"</body></html>"];

  LoadHtml(html);

  NSDictionary* expected = @{
    @"name" : @"TestForm",
    @"fields" : @[
      @{
        @"aria_description" : @"",
        @"aria_label" : @"",
        @"name" : @"firstname",
        @"name_attribute" : @"firstname",
        @"id_attribute" : @"firstname",
        @"identifier" : @"firstname",
        @"form_control_type" : @"text",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"John",
        @"label" : @"* First name:"
      },
      @{
        @"aria_description" : @"",
        @"aria_label" : @"",
        @"name" : @"vehicle",
        @"name_attribute" : @"vehicle",
        @"id_attribute" : @"vehicle1",
        @"identifier" : @"vehicle1",
        @"form_control_type" : @"checkbox",
        @"should_autocomplete" : @true,
        @"is_checkable" : @true,
        @"is_focusable" : @true,
        @"value" : @"Bike",
        @"label" : @"Bicycle"
      },
      @{
        @"aria_description" : @"",
        @"aria_label" : @"",
        @"name" : @"vehicle",
        @"name_attribute" : @"vehicle",
        @"id_attribute" : @"vehicle2",
        @"identifier" : @"vehicle2",
        @"form_control_type" : @"checkbox",
        @"should_autocomplete" : @true,
        @"is_checkable" : @true,
        @"is_focusable" : @true,
        @"value" : @"Car",
        @"label" : @"Automobile"
      },
      @{
        @"aria_description" : @"",
        @"aria_label" : @"",
        @"name" : @"vehicle",
        @"name_attribute" : @"vehicle",
        @"id_attribute" : @"vehicle3",
        @"identifier" : @"vehicle3",
        @"form_control_type" : @"checkbox",
        @"should_autocomplete" : @true,
        @"is_checkable" : @true,
        @"is_focusable" : @true,
        @"value" : @"Rocket",
        @"label" : @"Missile"
      },
      @{
        @"aria_description" : @"",
        @"aria_label" : @"",
        @"name" : @"nameintableth",
        @"name_attribute" : @"nameintableth",
        @"id_attribute" : @"nameintableth",
        @"identifier" : @"nameintableth",
        @"form_control_type" : @"text",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"John",
        @"label" : @"* First name:"
      },
      @{
        @"aria_description" : @"",
        @"aria_label" : @"",
        @"name" : @"emailtableth",
        @"name_attribute" : @"",
        @"id_attribute" : @"emailtableth",
        @"identifier" : @"emailtableth",
        @"form_control_type" : @"email",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @true,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"john@example.com",
        @"label" : @"Email:"
      },
      @{
        @"aria_description" : @"",
        @"aria_label" : @"",
        @"name" : @"pwd",
        @"name_attribute" : @"pwd",
        @"id_attribute" : @"pwd",
        @"identifier" : @"pwd",
        @"form_control_type" : @"password",
        @"autocomplete_attribute" : @"off",
        @"max_length" : GetDefaultMaxLength(),
        @"should_autocomplete" : @false,
        @"is_checkable" : @false,
        @"is_focusable" : @true,
        @"value" : @"",
        @"label" : @"* Password:"
      },
      @{
        @"aria_description" : @"",
        @"aria_label" : @"",
        @"name" : @"state",
        @"name_attribute" : @"state",
        @"id_attribute" : @"state",
        @"identifier" : @"state",
        @"form_control_type" : @"select-one",
        @"is_focusable" : @1,
        @"option_values" : @[ @"CA", @"TX" ],
        @"option_contents" : @[ @"California", @"Texas" ],
        @"should_autocomplete" : @1,
        @"value" : @"CA",
        @"label" : @"State:"
      }
    ]
  };

  NSString* result =
      ExecuteJavaScriptWithFormat(@"__gCrWeb.autofill.extractForms(%zu, true)",
                                  autofill::MinRequiredFieldsForHeuristics());
  NSArray* resultArray = [NSJSONSerialization
      JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  ASSERT_NSNE(nil, resultArray);
  EXPECT_EQ(1u, [resultArray count]);

  NSDictionary* form = [resultArray objectAtIndex:0];
  [expected enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL* stop) {
    EXPECT_NSEQ(form[key], obj);
  }];

  // Test with Object.prototype.toJSON override.
  result = ExecuteJavaScriptWithFormat(
      @"Object.prototype.toJSON=function(){return 'abcde';};"
       "__gCrWeb.autofill.extractForms(%zu, true)",
      autofill::MinRequiredFieldsForHeuristics());
  resultArray = [NSJSONSerialization
      JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  ASSERT_NSNE(nil, resultArray);

  form = [resultArray objectAtIndex:0];
  [expected enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL* stop) {
    EXPECT_NSEQ(form[key], obj);
  }];

  // Test with Array.prototype.toJSON override.
  result = ExecuteJavaScriptWithFormat(
      @"Array.prototype.toJSON=function(){return 'abcde';};"
       "__gCrWeb.autofill.extractForms(%zu, true)",
      autofill::MinRequiredFieldsForHeuristics());
  resultArray = [NSJSONSerialization
      JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                 options:0
                   error:nil];
  ASSERT_NSNE(nil, resultArray);

  form = [resultArray objectAtIndex:0];
  [expected enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL* stop) {
    EXPECT_NSEQ(form[key], obj);
  }];
}

TEST_F(AutofillControllerJsTest, FillActiveFormField) {
  LoadHtml(kHTMLForTestingElements);

  NSString* newValue = @"new value";
  EXPECT_NSEQ(newValue,
              ExecuteJavaScriptWithFormat(
                  @"var element=document.getElementsByName('lastname')[0];"
                   "element.focus();"
                   "var "
                   "data={\"name\":\"lastname\",\"value\":\"%@\","
                   "\"identifier\":\"lastname\"};"
                   "__gCrWeb.autofill.fillActiveFormField(data);"
                   "element.value",
                  newValue));

  EXPECT_NSEQ(@YES, ExecuteJavaScriptWithFormat(
                        @"var element=document.getElementsByName('gl')[0];"
                         "element.focus();"
                         "var oldValue = element.value;"
                         "var "
                         "data={\"name\":\"lastname\",\"value\":\"%@\","
                         "\"identifier\":\"lastname\"};"
                         "__gCrWeb.autofill.fillActiveFormField(data);"
                         "element.value === oldValue",
                        newValue))
      << "A non-form element's value should changed.";
}

// iOS version of FormAutofillTest.FormCache_ExtractNewForms from
// chrome/renderer/autofill/form_autofill_browsertest.cc
TEST_F(AutofillControllerJsTest, ExtractNewForms) {
  NSArray* testCases = @[
    // An empty form should not be extracted
    @{
      @"html" : @"<FORM name='TestForm' action='http://buh.com'>"
                 "</FORM>",
      @"expected_forms" : @0
    },
    // A form with less than three fields with no autocomplete type(s) should
    // not be extracted.
    @{
      @"html" : @"<FORM name='TestForm' action='http://buh.com'>"
                 "  <INPUT type='name' id='firstname'/>"
                 "</FORM>",
      @"expected_forms" : @0
    },
    // A form with less than three fields with at least one autocomplete type
    // should be extracted.
    @{
      @"html" : @"<FORM name='TestForm' action='http://buh.com'>"
                 "  <INPUT type='name' id='firstname'"
                 "         autocomplete='given-name'/>"
                 "</FORM>",
      @"expected_forms" : @1
    },
    // A form with three or more fields should be extracted.
    @{
      @"html" : @"<FORM name='TestForm' action='http://buh.com'>"
                 "  <INPUT type='text' id='firstname'/>"
                 "  <INPUT type='text' id='lastname'/>"
                 "  <INPUT type='text' id='email'/>"
                 "  <INPUT type='submit' value='Send'/>"
                 "</FORM>",
      @"expected_forms" : @1
    }
  ];

  for (NSDictionary* testCase in testCases) {
    LoadHtml(testCase[@"html"]);

    NSString* result = ExecuteJavaScriptWithFormat(
        @"__gCrWeb.autofill.extractForms(%zu, true)",
        autofill::MinRequiredFieldsForHeuristics());
    NSArray* resultArray = [NSJSONSerialization
        JSONObjectWithData:[result dataUsingEncoding:NSUTF8StringEncoding]
                   options:0
                     error:nil];
    ASSERT_NSNE(nil, resultArray);
    NSUInteger expectedCount =
        [testCase[@"expected_forms"] unsignedIntegerValue];
    EXPECT_EQ(expectedCount, [resultArray count])
        << base::SysNSStringToUTF8(testCase[@"html"]);
  }
}

// Test sanitizedFieldIsEmpty
TEST_F(AutofillControllerJsTest, SanitizedFieldIsEmpty) {
  LoadHtml(@"<html></html>");
  NSArray* tests = @[
    @[ @"--  (())//||__", @YES ], @[ @"  --  (())__  ", @YES ],
    @[ @"  --  (()c)__  ", @NO ], @[ @"123-456-7890", @NO ], @[ @"", @YES ]
  ];
  for (NSArray* test in tests) {
    NSString* result = ExecuteJavaScriptWithFormat(
        @"__gCrWeb.autofill.sanitizedFieldIsEmpty('%@');", test[0]);
    EXPECT_NSEQ(result, test[1]);
  }
}

}  // namespace
