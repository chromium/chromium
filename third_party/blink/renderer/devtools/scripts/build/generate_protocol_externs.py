#!/usr/bin/env python
# Copyright (c) 2011 Google Inc. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import os.path as path
import re
import sys
try:
    import json
except ImportError:
    import simplejson as json

sys.path.append(
    path.normpath(
        path.join(
            path.dirname(path.abspath(__file__)),
            os.pardir, os.pardir, os.pardir, os.pardir, os.pardir, 'inspector_protocol')))
import pdl  # pylint: disable=F0401

type_traits = {
    "any": "*",
    "string": "string",
    "binary": "string",
    "integer": "number",
    "number": "number",
    "boolean": "boolean",
    "array": "!Array<*>",
    "object": "!Object",
}

ref_types = {}


def full_qualified_type_id(domain_name, type_id):
    if type_id.find(".") == -1:
        return "%s.%s" % (domain_name, type_id)
    return type_id


def fix_camel_case(name):
    prefix = ""
    if name[0] == "-":
        prefix = "Negative"
        name = name[1:]
    refined = re.sub(r'-(\w)', lambda pat: pat.group(1).upper(), name)
    refined = to_title_case(refined)
    return prefix + re.sub(r'(?i)HTML|XML|WML|API', lambda pat: pat.group(0).upper(), refined)


def to_title_case(name):
    return name[:1].upper() + name[1:]


def generate_enum(name, json):
    enum_members = []
    for member in json["enum"]:
        enum_members.append("    %s: \"%s\"" % (fix_camel_case(member), member))
    return "\n/** @enum {string} */\n%s = {\n%s\n};\n" % (name, (",\n".join(enum_members)))


def param_type(domain_name, param):
    if "type" in param:
        if param["type"] == "array":
            items = param["items"]
            return "!Array<%s>" % param_type(domain_name, items)
        else:
            return type_traits[param["type"]]
    if "$ref" in param:
        type_id = full_qualified_type_id(domain_name, param["$ref"])
        if type_id in ref_types:
            return ref_types[type_id]
        else:
            print "Type not found: " + type_id
            return "!! Type not found: " + type_id


def param_name(param):
    name = param["name"]
    return name if name != "arguments" else "_arguments"


def load_schema(file, domains):
    input_file = open(file, "r")
    parsed_json = pdl.loads(input_file.read(), file)
    input_file.close()
    domains.extend(parsed_json["domains"])


def generate_protocol_externs(output_path, file1, file2):
    domains = []
    load_schema(file1, domains)
    load_schema(file2, domains)
    output_file = open(output_path, "w")

    for domain in domains:
        domain_name = domain["domain"]
        if "types" in domain:
            for type in domain["types"]:
                type_id = full_qualified_type_id(domain_name, type["id"])
                ref_types[type_id] = "Protocol.%s.%s" % (domain_name, type["id"])

    for domain in domains:
        domain_name = domain["domain"]

        output_file.write("Protocol.%s = {};\n" % domain_name)
        output_file.write("\n\n/**\n * @constructor\n*/\n")
        output_file.write("Protocol.%sAgent = function(){};\n" % domain_name)

        if "commands" in domain:
            for command in domain["commands"]:
                output_file.write("\n/**\n")
                params = []
                in_param_to_type = {}
                out_param_to_type = {}
                has_return_value = "returns" in command
                if "parameters" in command:
                    for in_param in command["parameters"]:
                        in_param_name = param_name(in_param)
                        if "optional" in in_param:
                            in_param_to_type[in_param_name] = "(%s|undefined)" % param_type(domain_name, in_param)
                            params.append("opt_%s" % in_param_name)
                            output_file.write(" * @param {%s=} opt_%s\n" % (param_type(domain_name, in_param), in_param_name))
                        else:
                            in_param_to_type[in_param_name] = param_type(domain_name, in_param)
                            params.append(in_param_name)
                            output_file.write(" * @param {%s} %s\n" % (param_type(domain_name, in_param), in_param_name))
                returns = []
                returns.append("?Protocol.Error")
                if ("error" in command):
                    returns.append("%s=" % param_type(domain_name, command["error"]))
                if (has_return_value):
                    for out_param in command["returns"]:
                        out_param_type = param_type(domain_name, out_param)
                        out_param_to_type[out_param["name"]] = out_param_type
                        if ("optional" in out_param):
                            returns.append("%s=" % out_param_type)
                        else:
                            returns.append("%s" % out_param_type)

                if has_return_value and len(command["returns"]) > 0:
                    out_param_type = param_type(domain_name, command["returns"][0])
                    if re.match(r"^[!?]", out_param_type[:1]):
                        out_param_type = out_param_type[1:]
                    out_param_type = "?%s" % out_param_type
                else:
                    out_param_type = "undefined"
                output_file.write(" * @return {!Promise<%s>}\n" % out_param_type)

                output_file.write(" */\n")
                output_file.write("Protocol.%sAgent.prototype.%s = function(%s) {};\n" %
                                  (domain_name, command["name"], ", ".join(params)))

                request_object_properties = []
                request_type = "Protocol.%sAgent.%sRequest" % (domain_name, to_title_case(command["name"]))
                for param in in_param_to_type:
                    request_object_properties.append("%s: %s" % (param, in_param_to_type[param]))
                if request_object_properties:
                    output_file.write("/** @typedef {!{%s}} */\n" % (", ".join(request_object_properties)))
                else:
                    output_file.write("/** @typedef {Object|undefined} */\n")
                output_file.write("%s;\n" % request_type)

                response_object_properties = []
                response_type = "Protocol.%sAgent.%sResponse" % (domain_name, to_title_case(command["name"]))
                for param in out_param_to_type:
                    response_object_properties.append("%s: %s" % (param, out_param_to_type[param]))
                if response_object_properties:
                    output_file.write("/** @typedef {!{%s}} */\n" % (", ".join(response_object_properties)))
                else:
                    output_file.write("/** @typedef {Object|undefined} */\n")
                output_file.write("%s;\n" % response_type)

                output_file.write("/**\n")
                output_file.write(" * @param {!%s} obj\n" % request_type)
                output_file.write(" * @return {!Promise<!%s>}" % response_type)
                output_file.write(" */\n")
                output_file.write("Protocol.%sAgent.prototype.invoke_%s = function(obj) {};\n" %
                                  (domain_name, command["name"]))

        if "types" in domain:
            for type in domain["types"]:
                if type["type"] == "object":
                    typedef_args = []
                    if "properties" in type:
                        for property in type["properties"]:
                            suffix = ""
                            if ("optional" in property):
                                suffix = "|undefined"
                            if "enum" in property:
                                enum_name = "Protocol.%s.%s%s" % (domain_name, type["id"], to_title_case(property["name"]))
                                output_file.write(generate_enum(enum_name, property))
                                typedef_args.append("%s:(%s%s)" % (property["name"], enum_name, suffix))
                            else:
                                typedef_args.append("%s:(%s%s)" % (property["name"], param_type(domain_name, property), suffix))
                    if (typedef_args):
                        output_file.write("\n/** @typedef {!{%s}} */\nProtocol.%s.%s;\n" %
                                          (", ".join(typedef_args), domain_name, type["id"]))
                    else:
                        output_file.write("\n/** @typedef {!Object} */\nProtocol.%s.%s;\n" % (domain_name, type["id"]))
                elif type["type"] == "string" and "enum" in type:
                    output_file.write(generate_enum("Protocol.%s.%s" % (domain_name, type["id"]), type))
                elif type["type"] == "array":
                    output_file.write("\n/** @typedef {!Array<!%s>} */\nProtocol.%s.%s;\n" %
                                      (param_type(domain_name, type["items"]), domain_name, type["id"]))
                else:
                    output_file.write("\n/** @typedef {%s} */\nProtocol.%s.%s;\n" %
                                      (type_traits[type["type"]], domain_name, type["id"]))

        if domain_name in ["Runtime", "Debugger", "HeapProfiler"]:
            output_file.write("/** @constructor */\n")
        else:
            output_file.write("/** @interface */\n")
        output_file.write("Protocol.%sDispatcher = function() {};\n" % domain_name)
        if "events" in domain:
            for event in domain["events"]:
                params = []
                if ("parameters" in event):
                    output_file.write("/**\n")
                    for param in event["parameters"]:
                        if ("optional" in param):
                            params.append("opt_%s" % param["name"])
                            output_file.write(" * @param {%s=} opt_%s\n" % (param_type(domain_name, param), param["name"]))
                        else:
                            params.append(param["name"])
                            output_file.write(" * @param {%s} %s\n" % (param_type(domain_name, param), param["name"]))
                    output_file.write(" */\n")
                output_file.write("Protocol.%sDispatcher.prototype.%s = function(%s) {};\n" %
                                  (domain_name, event["name"], ", ".join(params)))

    for domain in domains:
        domain_name = domain["domain"]
        uppercase_length = 0
        while uppercase_length < len(domain_name) and domain_name[uppercase_length].isupper():
            uppercase_length += 1

        output_file.write("/** @return {!Protocol.%sAgent}*/\n" % domain_name)
        output_file.write("Protocol.TargetBase.prototype.%s = function(){};\n" %
                          (domain_name[:uppercase_length].lower() + domain_name[uppercase_length:] + "Agent"))

        output_file.write("/**\n * @param {!Protocol.%sDispatcher} dispatcher\n */\n" % domain_name)
        output_file.write("Protocol.TargetBase.prototype.register%sDispatcher = function(dispatcher) {}\n" % domain_name)

    output_file.close()


if __name__ == "__main__":
    import sys
    import os.path
    program_name = os.path.basename(__file__)
    if len(sys.argv) < 5 or sys.argv[1] != "-o":
        sys.stderr.write("Usage: %s -o OUTPUT_FILE INPUT_FILE_1 INPUT_FILE_2\n" % program_name)
        exit(1)
    output_path = sys.argv[2]
    input_path_1 = sys.argv[3]
    input_path_2 = sys.argv[4]
    generate_protocol_externs(output_path, input_path_1, input_path_2)
