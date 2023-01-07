"""
Fenced Code Extension for Python Markdown
=========================================

This extension adds Fenced Code Blocks to Python-Markdown.

See <https://Python-Markdown.github.io/extensions/fenced_code_blocks>
for documentation.

Original code Copyright 2007-2008 [Waylan Limberg](http://achinghead.com/).


All changes Copyright 2008-2014 The Python Markdown Project

License: [BSD](https://opensource.org/licenses/bsd-license.php)
"""


from textwrap import dedent
from . import Extension
from ..preprocessors import Preprocessor
from .codehilite import CodeHilite, CodeHiliteExtension, parse_hl_lines
from .attr_list import get_attrs, AttrListExtension
from ..util import parseBoolValue
import re


class FencedCodeExtension(Extension):
    def __init__(self, **kwargs):
        self.config = {
            'lang_prefix': ['language-', 'Prefix prepended to the language. Default: "language-"']
        }
        super().__init__(**kwargs)

    def extendMarkdown(self, md):
        """ Add FencedBlockPreprocessor to the Markdown instance. """
        md.registerExtension(self)

        md.preprocessors.register(FencedBlockPreprocessor(md, self.getConfigs()), 'fenced_code_block', 25)


class FencedBlockPreprocessor(Preprocessor):
    FENCED_BLOCK_RE = re.compile(
        dedent(r'''
            (?P<fence>^(?:~{3,}|`{3,}))[ ]*                      # opening fence
            ((\{(?P<attrs>[^\}\n]*)\})?|                         # (optional {attrs} or
            (\.?(?P<lang>[\w#.+-]*))?[ ]*                        # optional (.)lang
            (hl_lines=(?P<quot>"|')(?P<hl_lines>.*?)(?P=quot))?) # optional hl_lines)
            [ ]*\n                                               # newline (end of opening fence)
            (?P<code>.*?)(?<=\n)                                 # the code block
            (?P=fence)[ ]*$                                      # closing fence
        '''),
        re.MULTILINE | re.DOTALL | re.VERBOSE
    )

    def __init__(self, md, config):
        super().__init__(md)
        self.config = config
        self.checked_for_deps = False
        self.codehilite_conf = {}
        self.use_attr_list = False
        # List of options to convert to bool values
        self.bool_options = [
            'linenums',
            'guess_lang',
            'noclasses',
            'use_pygments'
        ]

    def run(self, lines):
        """ Match and store Fenced Code Blocks in the HtmlStash. """

        # Check for dependent extensions
        if not self.checked_for_deps:
            for ext in self.md.registeredExtensions:
                if isinstance(ext, CodeHiliteExtension):
                    self.codehilite_conf = ext.getConfigs()
                if isinstance(ext, AttrListExtension):
                    self.use_attr_list = True

            self.checked_for_deps = True

        text = "\n".join(lines)
        while 1:
            m = self.FENCED_BLOCK_RE.search(text)
            if m:
                lang, id, classes, config = None, '', [], {}
                if m.group('attrs'):
                    id, classes, config = self.handle_attrs(get_attrs(m.group('attrs')))
                    if len(classes):
                        lang = classes.pop(0)
                else:
                    if m.group('lang'):
                        lang = m.group('lang')
                    if m.group('hl_lines'):
                        # Support hl_lines outside of attrs for backward-compatibility
                        config['hl_lines'] = parse_hl_lines(m.group('hl_lines'))

                # If config is not empty, then the codehighlite extension
                # is enabled, so we call it to highlight the code
                if self.codehilite_conf and self.codehilite_conf['use_pygments'] and config.get('use_pygments', True):
                    local_config = self.codehilite_conf.copy()
                    local_config.update(config)
                    # Combine classes with cssclass. Ensure cssclass is at end
                    # as pygments appends a suffix under certain circumstances.
                    # Ignore ID as Pygments does not offer an option to set it.
                    if classes:
                        local_config['css_class'] = '{} {}'.format(
                            ' '.join(classes),
                            local_config['css_class']
                        )
                    highliter = CodeHilite(
                        m.group('code'),
                        lang=lang,
                        style=local_config.pop('pygments_style', 'default'),
                        **local_config
                    )

                    code = highliter.hilite()
                else:
                    id_attr = lang_attr = class_attr = kv_pairs = ''
                    if lang:
                        lang_attr = ' class="{}{}"'.format(self.config.get('lang_prefix', 'language-'), lang)
                    if classes:
                        class_attr = ' class="{}"'.format(' '.join(classes))
                    if id:
                        id_attr = ' id="{}"'.format(id)
                    if self.use_attr_list and config and not config.get('use_pygments', False):
                        # Only assign key/value pairs to code element if attr_list ext is enabled, key/value pairs
                        # were defined on the code block, and the `use_pygments` key was not set to True. The
                        # `use_pygments` key could be either set to False or not defined. It is omitted from output.
                        kv_pairs = ' ' + ' '.join(
                            '{k}="{v}"'.format(k=k, v=v) for k, v in config.items() if k != 'use_pygments'
                        )
                    code = '<pre{id}{cls}><code{lang}{kv}>{code}</code></pre>'.format(
                        id=id_attr,
                        cls=class_attr,
                        lang=lang_attr,
                        kv=kv_pairs,
                        code=self._escape(m.group('code'))
                    )

                placeholder = self.md.htmlStash.store(code)
                text = '{}\n{}\n{}'.format(text[:m.start()],
                                           placeholder,
                                           text[m.end():])
            else:
                break
        return text.split("\n")

    def handle_attrs(self, attrs):
        """ Return tuple: (id, [list, of, classes], {configs}) """
        id = ''
        classes = []
        configs = {}
        for k, v in attrs:
            if k == 'id':
                id = v
            elif k == '.':
                classes.append(v)
            elif k == 'hl_lines':
                configs[k] = parse_hl_lines(v)
            elif k in self.bool_options:
                configs[k] = parseBoolValue(v, fail_on_errors=False, preserve_none=True)
            else:
                configs[k] = v
        return id, classes, configs

    def _escape(self, txt):
        """ basic html escaping """
        txt = txt.replace('&', '&amp;')
        txt = txt.replace('<', '&lt;')
        txt = txt.replace('>', '&gt;')
        txt = txt.replace('"', '&quot;')
        return txt


def makeExtension(**kwargs):  # pragma: no cover
    return FencedCodeExtension(**kwargs)
