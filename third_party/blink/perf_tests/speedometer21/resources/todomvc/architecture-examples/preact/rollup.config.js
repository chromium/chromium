import nodeResolve from 'rollup-plugin-node-resolve';
import commonjs from 'rollup-plugin-commonjs';
import babel from 'rollup-plugin-babel';

export default {
    entry: 'src/index.js',
    dest: 'dist/app.js',
    format: 'iife',
    sourceMap: true,
    external: [],
    plugins: [
        babel({
            babelrc: false,
            presets: [
                ['es2015', { loose:true, modules:false }],
                'stage-0'
            ],
            plugins: [
                ['transform-react-jsx', { pragma:'h' }]
            ]
        }),
        nodeResolve({
            jsnext: true
        }),
        commonjs()
    ]
};
