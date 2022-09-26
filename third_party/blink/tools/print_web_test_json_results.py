#!/usr/bin/env vpython3
import json
import optparse
import os
import sys

from blinkpy.common.host import Host
from blinkpy.web_tests.port.factory import platform_options, configuration_options


def main(argv):
    parser = optparse.OptionParser(usage='%prog [path-to-results.json]')
    parser.add_option(
        '--failures', action='store_true', help='show failing tests')
    parser.add_option('--flakes', action='store_true', help='show flaky tests')
    parser.add_option(
        '--expected',
        action='store_true',
        help='include expected results along with unexpected')
    parser.add_option(
        '--passes', action='store_true', help='show passing tests')
    parser.add_option(
        '--ignored-failures-path',
        action='store',
        help='ignore failures seen in a previous run')
    parser.add_options(platform_options())
    parser.add_options(configuration_options())
    options, args = parser.parse_args(argv)

    host = Host()
    if args:
        if args[0] == '-':
            txt = sys.stdin.read()
        elif os.path.exists(args[0]):
            with open(args[0], 'r') as fp:
                txt = fp.read()
        else:
            print("file not found: %s" % args[0], file=sys.stderr)
            sys.exit(1)
    else:
        txt = host.filesystem.read_text_file(
            host.filesystem.join(
                host.port_factory.get(options=options).artifacts_directory(),
                'full_results.json'))

    if txt.startswith('ADD_RESULTS(') and txt.endswith(');'):
        txt = txt[12:-2]  # ignore optional JSONP wrapper
    results = json.loads(txt)

    passes, failures, flakes = decode_results(results, options.expected)

    tests_to_print = []
    if options.passes:
        tests_to_print += passes.keys()
    if options.failures:
        tests_to_print += failures.keys()
    if options.flakes:
        tests_to_print += flakes.keys()
    print("\n".join(sorted(tests_to_print)))

    if options.ignored_failures_path:
        with open(options.ignored_failures_path, 'r') as fp:
            txt = fp.read()
        if txt.startswith('ADD_RESULTS(') and txt.endswith(');'):
            txt = txt[12:-2]  # ignore optional JSONP wrapper
        results = json.loads(txt)
        _, ignored_failures, _ = decode_results(results, options.expected)
        new_failures = set(failures.keys()) - set(ignored_failures.keys())
        if new_failures:
            print("New failures:")
            print("\n".join(sorted(new_failures)))
            print
        if ignored_failures:
            print("Ignored failures:")
            print("\n".join(sorted(ignored_failures.keys())))
        if new_failures:
            return 1
        return 0


def decode_results(results, include_expected=False):
    tests = convert_trie_to_flat_paths(results['tests'])
    failures = {}
    flakes = {}
    passes = {}
    for (test, result) in tests.items():
        if include_expected or result.get('is_unexpected'):
            actual_results = result['actual'].split()
            expected_results = result['expected'].split()
            if len(actual_results) > 1:
                if actual_results[1] in expected_results:
                    flakes[test] = actual_results[0]
                else:
                    # We report the first failure type back, even if the second
                    # was more severe.
                    failures[test] = actual_results[0]
            elif actual_results[0] == 'PASS':
                passes[test] = result
            else:
                failures[test] = actual_results[0]

    return (passes, failures, flakes)


def convert_trie_to_flat_paths(trie, prefix=None):
    # Cloned from blinkpy.web_tests.layout_package.json_results_generator
    # so that this code can stand alone.
    result = {}
    for name, data in trie.iteritems():
        if prefix:
            name = prefix + "/" + name

        if len(data) and not "actual" in data and not "expected" in data:
            result.update(convert_trie_to_flat_paths(data, name))
        else:
            result[name] = data

    return result


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
