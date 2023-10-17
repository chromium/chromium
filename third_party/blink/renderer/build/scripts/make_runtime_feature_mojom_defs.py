import json5_generator
import make_runtime_features
import make_runtime_features_utilities as util
import template_expander


class RuntimeFeatureMojomWriter(make_runtime_features.BaseRuntimeFeatureWriter
                                ):
    file_basename = "runtime_feature"

    def __init__(self, json5_file_path, output_dir):
        super(RuntimeFeatureMojomWriter,
              self).__init__(json5_file_path, output_dir)
        self._outputs = {
            (self.file_basename + '.mojom'): self.generate_mojom_definition
        }
        self._overridable_features = util.overridable_features(self._features)

    def _template_inputs(self):
        return {
            'features': self._features,
            'overridable_features': self._overridable_features,
            'platforms': self._platforms(),
            'input_files': self._input_files,
            'header_guard': self._header_guard,
        }

    @template_expander.use_jinja(f'templates/{file_basename}.mojom.tmpl')
    def generate_mojom_definition(self):
        return self._template_inputs()


if __name__ == '__main__':
    json5_generator.Maker(RuntimeFeatureMojomWriter).main()
