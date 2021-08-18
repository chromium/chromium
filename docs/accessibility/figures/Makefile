all: \
	ax_tree.png \
	generated_content.png \
	hidden_content.png \
	cover_flow.png \
	single_process.png \
	single_process_css_system.png \
	single_process_system.png

%.png: %.gv Makefile
	dot -Tpng $< -o $@
