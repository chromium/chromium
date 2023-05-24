import ExtractTextPlugin from 'extract-text-webpack-plugin';
import ReplacePlugin from 'replace-bundle-webpack-plugin';

module.exports = {
    entry: {
        app: './src/index.js',
        'todomvc-common': 'todomvc-common'
    },
    output: {
        path: './dist',
        filename: '[name].js'
    },
    module: {
        loaders: [
            {
                test: /\.jsx?$/,
                exclude: /node_modules/,
                loader: 'babel'
            },
            {
                test: /\.css$/,
                loader: ExtractTextPlugin.extract('style', 'css?sourceMap')
            }
        ]
    },
    plugins: [
        new ExtractTextPlugin('style.css', { allChunks: true }),
        new ReplacePlugin([{
            // this is actually the property name https://github.com/kimhou/replace-bundle-webpack-plugin/issues/1
            partten: /throw\s+(new\s+)?[a-zA-Z]+Error\s*\(/g,
            replacement: () => 'return;('
        }])
    ],
    devtool: 'source-map',
    devServer: {
        port: process.env.PORT || 8080,
        contentBase: './src'
    }
};
