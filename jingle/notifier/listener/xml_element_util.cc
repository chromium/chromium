// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/listener/xml_element_util.h"

#include <sstream>
#include <string>

#include "base/strings/string_number_conversions.h"
#include "third_party/libjingle_xmpp/xmllite/qname.h"
#include "third_party/libjingle_xmpp/xmllite/xmlconstants.h"
#include "third_party/libjingle_xmpp/xmllite/xmlelement.h"
#include "third_party/libjingle_xmpp/xmllite/xmlprinter.h"

namespace notifier {

std::string XmlElementToString(const jingle_xmpp::XmlElement& xml_element) {
  std::ostringstream xml_stream;
  jingle_xmpp::XmlPrinter::PrintXml(&xml_stream, &xml_element);
  return xml_stream.str();
}

jingle_xmpp::XmlElement* MakeBoolXmlElement(const char* name, bool value) {
  const jingle_xmpp::QName elementQName(jingle_xmpp::STR_EMPTY, name);
  const jingle_xmpp::QName boolAttrQName(jingle_xmpp::STR_EMPTY, "bool");
  jingle_xmpp::XmlElement* bool_xml_element =
      new jingle_xmpp::XmlElement(elementQName, true);
  bool_xml_element->AddAttr(boolAttrQName, value ? "true" : "false");
  return bool_xml_element;
}

jingle_xmpp::XmlElement* MakeIntXmlElement(const char* name, int value) {
  const jingle_xmpp::QName elementQName(jingle_xmpp::STR_EMPTY, name);
  const jingle_xmpp::QName intAttrQName(jingle_xmpp::STR_EMPTY, "int");
  jingle_xmpp::XmlElement* int_xml_element =
      new jingle_xmpp::XmlElement(elementQName, true);
  int_xml_element->AddAttr(intAttrQName, base::NumberToString(value));
  return int_xml_element;
}

jingle_xmpp::XmlElement* MakeStringXmlElement(const char* name, const char* value) {
  const jingle_xmpp::QName elementQName(jingle_xmpp::STR_EMPTY, name);
  const jingle_xmpp::QName dataAttrQName(jingle_xmpp::STR_EMPTY, "data");
  jingle_xmpp::XmlElement* data_xml_element =
      new jingle_xmpp::XmlElement(elementQName, true);
  data_xml_element->AddAttr(dataAttrQName, value);
  return data_xml_element;
}

}  // namespace notifier
