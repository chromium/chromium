import json
import os

from blinkpy.common.host_mock import MockHost
from blinkpy.common.system.log_testing import LoggingTestCase
from blinkpy.w3c.wpt_results_processor import WPTResultsProcessor


class WPTResultsProcessorTest(LoggingTestCase):
    def setUp(self):
        super(WPTResultsProcessorTest, self).setUp()
        self.host = MockHost()
        self.fs = self.host.filesystem
        self.processor = WPTResultsProcessor(
            self.host,
            web_tests_dir=os.path.join('third_party', 'blink', 'web_tests'),
            artifacts_dir=os.path.join('out', 'Default',
                                       'layout-test-results'),
        )
        self.wpt_report_path = os.path.join('out', 'Default',
                                            'wpt_report.json')

        version_path = os.path.join(self.processor.web_tests_dir, 'external',
                                    'Version')
        with self.fs.open_text_file_for_writing(version_path) as version_file:
            version_file.write('Version: ')
            version_file.write('afd66ac5976672821b2788cd5f6ae57701240308')
            version_file.write('\n')

        report = {
            'run_info': {
                'os': 'linux',
                'os_version': '18.04',
                'product': 'chrome',
                'revision': '57a5dfb2d7d6253fbb7dbd7c43e7588f9339f431',
            },
            'results': [],
        }
        report_file = self.fs.open_text_file_for_writing(self.wpt_report_path)
        with report_file:
            json.dump(report, report_file)

    def test_process_wpt_report(self):
        output_path = self.processor.process_wpt_report(self.wpt_report_path)
        self.assertEqual(os.path.dirname(output_path),
                         self.processor.artifacts_dir)
        with self.fs.open_text_file_for_reading(output_path) as output_file:
            report = json.load(output_file)
        self.assertEqual(report['run_info']['os'], 'linux')
        self.assertEqual(report['run_info']['os_version'], '18.04')
        self.assertEqual(report['run_info']['product'], 'chrome')
        self.assertEqual(report['run_info']['revision'],
                         'afd66ac5976672821b2788cd5f6ae57701240308')
