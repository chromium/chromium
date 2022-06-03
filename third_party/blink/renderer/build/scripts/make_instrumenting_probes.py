# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import ast
from functools import partial
import optparse
import os.path
import re
import sys

# Path handling for libraries and templates
# Paths have to be normalized because Jinja uses the exact template path to
# determine the hash used in the cache filename, and we need a pre-caching step
# to be concurrency-safe. Use absolute path because __file__ is absolute if
# module is imported, and relative if executed directly.
# If paths differ between pre-caching and individual file compilation, the cache
# is regenerated, which causes a race condition and breaks concurrent build,
# since some compile processes will try to read the partially written cache.
_MODULE_PATH, _ = os.path.split(os.path.realpath(__file__))
_THIRD_PARTY_DIR = os.path.normpath(
    os.path.join(_MODULE_PATH, os.pardir, os.pardir, os.pardir, os.pardir))
# jinja2 is in chromium's third_party directory.
# Insert at 1 so at front to override system libraries, and
# after path[0] == invoking script dir
sys.path.insert(1, _THIRD_PARTY_DIR)
import jinja2

from blinkbuild.name_style_converter import NameStyleConverter


def _json5_loads(lines):
    # Use json5.loads when json5 is available. Currently we use simple
    # regexs to convert well-formed JSON5 to PYL format.
    # Strip away comments and quote unquoted keys.
    re_comment = re.compile(r"^\s*//.*$|//+ .*$", re.MULTILINE)
    re_map_keys = re.compile(r"^\s*([$A-Za-z_][\w]*)\s*:", re.MULTILINE)
    pyl = re.sub(re_map_keys, r"'\1':", re.sub(re_comment, "", lines))
    # Convert map values of true/false to Python version True/False.
    re_true = re.compile(r":\s*true\b")
    re_false = re.compile(r":\s*false\b")
    pyl = re.sub(re_true, ":True", re.sub(re_false, ":False", pyl))
    return ast.literal_eval(pyl)


def to_singular(text):
    return text[:-1] if text[-1] == "s" else text


def to_snake_case(name):
    return NameStyleConverter(name).to_snake_case()


def agent_config(config, agent_name, field):
    return config["observers"].get(agent_name, {}).get(field)


def agent_name_to_class(config, agent_name):
    return agent_config(config, agent_name, "class") or agent_name


def agent_name_to_include(config, agent_name):
    include_path = agent_config(
        config, agent_name,
        "include_path") or config["settings"]["include_path"]
    agent_class = agent_name_to_class(config, agent_name)
    include_file = os.path.join(
        include_path,
        NameStyleConverter(agent_class).to_snake_case() + ".h")
    return include_file.replace("dev_tools", "devtools")


def initialize_jinja_env(config, cache_dir):
    jinja_env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(
            os.path.join(_MODULE_PATH, "templates")),
        # Bytecode cache is not concurrency-safe unless pre-cached:
        # if pre-cached this is read-only, but writing creates a race condition.
        bytecode_cache=jinja2.FileSystemBytecodeCache(cache_dir),
        keep_trailing_newline=True,  # newline-terminate generated files
        lstrip_blocks=True,  # so can indent control flow tags
        trim_blocks=True)
    jinja_env.filters.update({
        "to_snake_case":
        to_snake_case,
        "to_singular":
        to_singular,
        "agent_name_to_class":
        partial(agent_name_to_class, config),
        "agent_name_to_include":
        partial(agent_name_to_include, config)
    })
    jinja_env.add_extension('jinja2.ext.loopcontrols')
    return jinja_env


def match_and_consume(pattern, source):
    match = re.match(pattern, source)
    if match:
        return match, source[len(match.group(0)):].strip()
    return None, source


def load_model_from_idl(source):
    source = re.sub(r"//.*", "", source)  # Remove line comments
    # Remove block comments
    source = re.sub(r"/\*(.|\n)*?\*/", "", source, re.MULTILINE)
    # Merge the method annotation with the next line
    source = re.sub(r"\]\s*?\n\s*", "] ", source)
    source = source.strip()
    model = []
    while len(source):
        match, source = match_and_consume(r"interface\s(\w*)\s?\{([^\{]*)\}",
                                          source)
        if not match:
            sys.stderr.write("Cannot parse %s\n" % source[:100])
            sys.exit(1)
        model.append(File(match.group(1), match.group(2)))
    return model


class File(object):
    def __init__(self, name, source):
        self.name = NameStyleConverter(name).to_snake_case()
        self.header_name = self.name + "_inl.h"
        self.forward_declarations = []
        self.declarations = []
        for line in map(str.strip, source.split("\n")):
            line = re.sub(r"\s{2,}", " ", line).strip()  # Collapse whitespace
            if len(line) == 0:
                continue
            elif line.startswith("class ") or line.startswith("struct "):
                self.forward_declarations.append(line)
            else:
                self.declarations.append(Method(line))
        self.forward_declarations.sort()


class Method(object):
    def __init__(self, source):
        match = re.match(r"(?:(\w+\*?)\s+)?(\w+)\s*\((.*)\)\s*;", source)
        if not match:
            sys.stderr.write("Cannot parse %s\n" % source)
            sys.exit(1)

        self.name = match.group(2)
        self.is_scoped = not match.group(1)
        if not self.is_scoped and match.group(1) != "void":
            raise Exception("Instant probe must return void: %s" % self.name)

        # Splitting parameters by a comma, assuming that attribute
        # lists contain no more than one attribute.
        self.params = list(
            map(Parameter, map(str.strip,
                               match.group(3).split(","))))


class Parameter(object):
    def __init__(self, source):
        self.options = []
        match, source = match_and_consume(r"\[(\w*)\]", source)
        if match:
            self.options.append(match.group(1))

        parts = list(map(str.strip, source.split("=")))
        self.default_value = parts[1] if len(parts) != 1 else None

        param_decl = parts[0]
        min_type_tokens = 2 if re.match("(const|unsigned long) ",
                                        param_decl) else 1

        if len(param_decl.split(" ")) > min_type_tokens:
            parts = param_decl.split(" ")
            self.type = " ".join(parts[:-1])
            self.name = parts[-1]
        else:
            self.type = param_decl
            self.name = build_param_name(self.type)

        if self.type[-1] == "*" and "char" not in self.type:
            self.member_type = "Member<%s>" % self.type[:-1]
        else:
            self.member_type = self.type


def build_param_name(param_type):
    return "param_" + NameStyleConverter(
        re.match(r"(const |scoped_refptr<)?(\w*)",
                 param_type).group(2)).to_snake_case()


def load_config(file_name):
    default_config = {"settings": {}, "observers": {}}
    if not file_name:
        return default_config
    with open(file_name) as config_file:
        return _json5_loads(config_file.read()) or default_config


def build_observers(config, files):
    all_pidl_probes = set()
    for f in files:
        probes = set([probe.name for probe in f.declarations])
        if all_pidl_probes & probes:
            raise Exception(
                "Multiple probe declarations: %s" % all_pidl_probes & probes)
        all_pidl_probes |= probes

    all_observers = set()
    observers_by_probe = {}
    unused_probes = set(all_pidl_probes)
    for observer_name in config["observers"]:
        all_observers.add(observer_name)
        observer = config["observers"][observer_name]
        for probe in observer["probes"]:
            unused_probes.discard(probe)
            if probe not in all_pidl_probes:
                raise Exception(
                    'Probe %s is not declared in PIDL file' % probe)
            observers_by_probe.setdefault(probe, set()).add(observer_name)
    if unused_probes:
        raise Exception("Unused probes: %s" % unused_probes)

    for f in files:
        for probe in f.declarations:
            probe.agents = observers_by_probe[probe.name]
    return all_observers


def main():
    cmdline_parser = optparse.OptionParser()
    cmdline_parser.add_option("--output_dir")
    cmdline_parser.add_option("--config")

    try:
        arg_options, arg_values = cmdline_parser.parse_args()
        if len(arg_values) != 1:
            raise ValueError("Exactly one plain argument expected (found %s)" %
                             len(arg_values))
        input_path = arg_values[0]
        output_dirpath = arg_options.output_dir
        if not output_dirpath:
            raise ValueError("Output directory must be specified")
        config_file_name = arg_options.config
    except ValueError:
        # Work with python 2 and 3 http://docs.python.org/py3k/howto/pyporting.html
        exc = sys.exc_info()[1]
        sys.stderr.write(
            "Failed to parse command-line arguments: %s\n\n" % exc)
        sys.stderr.write("Usage: <script> [options] <probes.pidl>\n")
        sys.stderr.write("Options:\n")
        sys.stderr.write("\t--config <config_file.json5>\n")
        sys.stderr.write("\t--output_dir <output_dir>\n")
        exit(1)

    match = re.search(r"\bgen[\\/]", output_dirpath)
    if match:
        output_path_in_gen_dir = output_dirpath[match.end():].replace(
            os.path.sep, '/') + '/'
    else:
        output_path_in_gen_dir = ''

    config = load_config(config_file_name)
    jinja_env = initialize_jinja_env(config, output_dirpath)
    base_name = os.path.splitext(os.path.basename(input_path))[0]

    fin = open(input_path, "r")
    files = load_model_from_idl(fin.read())
    fin.close()

    template_context = {
        "files": files,
        "agents": build_observers(config, files),
        "config": config,
        "method_name":
        lambda name: NameStyleConverter(name).to_function_name(),
        "name": NameStyleConverter(base_name).to_upper_camel_case(),
        "header": base_name,
        "input_files": [os.path.basename(input_path)],
        "output_path_in_gen_dir": output_path_in_gen_dir
    }

    template_context["template_file"] = "/instrumenting_probes_impl.cc.tmpl"
    cpp_template = jinja_env.get_template(template_context["template_file"])
    cpp_file = open(output_dirpath + "/" + base_name + "_impl.cc", "w")
    cpp_file.write(cpp_template.render(template_context))
    cpp_file.close()

    template_context["template_file"] = "/probe_sink.h.tmpl"
    sink_h_template = jinja_env.get_template(template_context["template_file"])
    sink_h_file_name = to_singular(base_name) + "_sink.h"
    sink_h_file = open(output_dirpath + "/" + sink_h_file_name, "w")
    template_context["header_guard"] = NameStyleConverter(
        output_path_in_gen_dir + "/" + sink_h_file_name).to_header_guard()
    sink_h_file.write(sink_h_template.render(template_context))
    sink_h_file.close()

    for f in files:
        template_context["file"] = f
        template_context["template_file"] = "/instrumenting_probes_inl.h.tmpl"
        template_context["header_guard"] = NameStyleConverter(
            output_path_in_gen_dir + "/" + f.header_name).to_header_guard()
        h_template = jinja_env.get_template(template_context["template_file"])
        h_file = open(output_dirpath + "/" + f.header_name, "w")
        h_file.write(h_template.render(template_context))
        h_file.close()


if __name__ == "__main__":
    main()
